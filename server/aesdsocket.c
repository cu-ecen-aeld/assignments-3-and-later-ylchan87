#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <signal.h>

#include <sys/syscall.h>

#include "aesdsocket.h"

volatile sig_atomic_t interrupted = 0;

void sig_handler(int signame){
    interrupted = 1;
}

void* timer_work(void* thread_param)
{
    struct Per_connection_data* per_connection_data = (struct Per_connection_data *) thread_param;

    FILE* fp                        = per_connection_data->fp;
    pthread_mutex_t* file_use_mutex = per_connection_data->file_use_mutex;

    int rc;

    char outstr[200];
    size_t bytes_written;

    time_t t;
    struct tm *tmp;

    while (1){
        sleep(10);
        if (interrupted) break;

        t = time(NULL);
        tmp = localtime(&t);        
        bytes_written = strftime(outstr, sizeof(outstr), "timestamp:%a, %d %b %Y %T %z\n", tmp);

        rc = pthread_mutex_lock(file_use_mutex);
        fseek(fp, 0, SEEK_END);
        fwrite(outstr , sizeof(char) , bytes_written , fp );
        rc = pthread_mutex_unlock(file_use_mutex);
    }
}

void* per_connection_work(void* thread_param)
{
    struct Per_connection_data* per_connection_data = (struct Per_connection_data *) thread_param;

    FILE* fp                        = per_connection_data->fp;
    pthread_mutex_t* file_use_mutex = per_connection_data->file_use_mutex;
    int connection_fd               = per_connection_data->connection_fd;
    char* peerIP                    = per_connection_data->peerIP;

    per_connection_data->thread_complete_success = false;

    int rc = 0;

    pid_t pid = syscall(__NR_gettid);
    per_connection_data->pid = pid;

    printf("Work thread %d start for %s\n", pid, peerIP);

    char read_buffer[256];
    ssize_t bytes_read = 0;
    while (1){
        bytes_read = recv(connection_fd, read_buffer, sizeof(read_buffer)-1, 0);
        if (interrupted) break;

        rc = pthread_mutex_lock(file_use_mutex);
        fseek(fp, 0, SEEK_END);
        fwrite(read_buffer , sizeof(char) , bytes_read , fp );
        rc = pthread_mutex_unlock(file_use_mutex);

        // FIXME? we assume \n only appears at the last char of the read
        if (read_buffer[bytes_read-1] == '\n') break;
    }
    printf("Receive data done for connection from %s\n", peerIP);

    // echo all contents received so far back to client
    rc = pthread_mutex_lock(file_use_mutex);
    rewind(fp);
    while (1) {
        bytes_read = fread(read_buffer, sizeof(char), sizeof(read_buffer), fp);
        if (bytes_read == 0) break;

        send(connection_fd, read_buffer, bytes_read, 0);
    };
    rc = pthread_mutex_unlock(file_use_mutex);

    close(connection_fd); connection_fd = -1;

    printf("Closed connection from %s\n", peerIP);
    syslog(LOG_INFO, "Closed connection from %s\n", peerIP);


    per_connection_data->thread_complete_success = true;
    return thread_param;
}

int main( int argc, char *argv[] )  {
    openlog(NULL, 0, LOG_USER);

    struct sigaction new_action;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;
    new_action.sa_handler = sig_handler;
    sigaction (SIGINT , &new_action, NULL);
    sigaction (SIGTERM, &new_action, NULL);


    int ret_code;
    int status;

    int socket_fd = socket(PF_INET, SOCK_STREAM, 0);

    // so that can reuse the same port after restarting the prog
    // ref: https://hea-www.harvard.edu/~fine/Tech/addrinuse.html
    const int enable = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        fprintf(stderr,"setsockopt(SO_REUSEADDR) failed\n");
        exit(1);
    }

    // bind the socket
    {
        struct addrinfo hints;
        struct addrinfo *filled_addrinfo;  // will point to the results

        memset(&hints, 0, sizeof hints); // make sure the struct is empty
        hints.ai_flags  = AI_PASSIVE;
        hints.ai_family = AF_INET;

        ret_code = getaddrinfo(NULL, "9000", &hints, &filled_addrinfo);
        if (ret_code != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(ret_code));
            exit(1);
        }

        ret_code = bind(socket_fd, filled_addrinfo->ai_addr, filled_addrinfo->ai_addrlen);
        if (ret_code<0){
            fprintf(stderr, "bind error: %s\n", strerror(errno));
            exit(1);
        }

        freeaddrinfo(filled_addrinfo); // free the linked-list
    }

    if (argc>1 && strcmp(argv[1], "-d")==0){
        fprintf(stdout, "daemonize\n");
        daemon(0,0);
    }

    FILE* fp = NULL;
    pthread_mutex_t file_use_mutex;
    pthread_mutex_init(&file_use_mutex, NULL);

    // remove("/var/tmp/aesdsocketdata");
    fp = fopen( "/var/tmp/aesdsocketdata" , "w+" );
    if (fp==NULL){
        fprintf(stderr, "open file failed: %s\n", strerror(errno));
        exit(1);
    }

    // setup timer thread
    pthread_t timer_thread;
    struct Per_connection_data timer_thread_data;
    timer_thread_data.fp = fp;
    timer_thread_data.file_use_mutex = &file_use_mutex;
    int rc = pthread_create(&timer_thread, NULL, &timer_work, &timer_thread_data);




    SLIST_HEAD(SList_connection_head, SList_connection) connections;
    SLIST_INIT(&connections);
    struct SList_connection *connection, *next_connection;

    while (1){

        // listen and wait for client
        ret_code = listen(socket_fd, 10);
        if (ret_code<0){
            fprintf(stderr, "listen connection error: %s\n", strerror(errno));
            break;
        }

        // client connected, get fd from it
        struct sockaddr_in connection_addr; // sockaddr_in is interchangable with sockaddr ref: https://beej.us/guide/bgnet/html/#structs
        int                connection_addr_len = sizeof connection_addr;

        int connection_fd = accept(socket_fd, (struct sockaddr *) &connection_addr, &connection_addr_len);
        if (connection_fd<0){
            fprintf(stderr, "accept connection error: %s\n", strerror(errno));
            break;
        }

        struct Per_connection_data* per_connection_data = (struct Per_connection_data*) malloc(sizeof(struct Per_connection_data));
        per_connection_data->fp = fp;
        per_connection_data->file_use_mutex = &file_use_mutex;
        per_connection_data->connection_fd = connection_fd;
        per_connection_data->thread_complete_success = false;
        inet_ntop(AF_INET, &(connection_addr.sin_addr), per_connection_data->peerIP, INET_ADDRSTRLEN);

        printf("Accepted connection from %s\n", per_connection_data->peerIP);
        syslog(LOG_INFO, "Accepted connection from %s\n", per_connection_data->peerIP);


        // prune completed threads
        while(1){
            connection = SLIST_FIRST(&connections);
            if (connection == NULL) break;
            if (!connection->data->thread_complete_success) break;
            
            SLIST_REMOVE_HEAD(&connections, entries);
            printf("Join completed thread %d \n", connection->data->pid);
            pthread_join(connection->thread, NULL);
            printf("Join completed thread %d done \n", connection->data->pid);
            free(connection->data);
            free(connection);
        }

        while (1){
            if (connection == NULL) break;

            next_connection = SLIST_NEXT(connection, entries);
            if (next_connection == NULL) break;

            if (next_connection->data->thread_complete_success){
                connection->entries.sle_next = next_connection->entries.sle_next;

                printf("Join completed thread %d \n", next_connection->data->pid);
                pthread_join(next_connection->thread, NULL);
                printf("Join completed thread %d joined \n", next_connection->data->pid);
                free(next_connection->data);
                free(next_connection);
            }
            else{
                connection = SLIST_NEXT(connection, entries);
            }

        }


        // add connection to linked list
        connection = malloc(sizeof(struct SList_connection));

        int rc = pthread_create(&connection->thread, NULL, &per_connection_work, per_connection_data);

        if (rc == 0){
            connection->data   = per_connection_data;
            SLIST_INSERT_HEAD(&connections, connection, entries);
            printf("Add work thread %ld\n", connection->thread);
        }
        else{
            free(per_connection_data);
            free(connection);
        }

    }

    if (interrupted){
        printf("Caught signal, exiting\n");
        syslog(LOG_INFO, "Caught signal, exiting\n");
    }
    else{

        printf("Exiting due to other errors\n");
    }

    while(!SLIST_EMPTY(&connections)){
        connection = SLIST_FIRST(&connections);
        SLIST_REMOVE_HEAD(&connections, entries);
        pthread_join(connection->thread, NULL);
        free(connection->data);
        free(connection);
    }

    pthread_kill(timer_thread, SIGINT);
    pthread_join(timer_thread, NULL);

    if (fp) fclose(fp);
    if (    socket_fd>=0) close(    socket_fd);
    remove("/var/tmp/aesdsocketdata");

    pthread_mutex_destroy(&file_use_mutex);
    return 0;
}