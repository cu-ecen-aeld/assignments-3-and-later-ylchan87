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

volatile sig_atomic_t interrupted = 0;

void sig_handler(int signame){
    interrupted = 1;
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
        hints.ai_flags = AI_PASSIVE;

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

    int connection_fd = -1;
    FILE* fp = NULL;
    
    remove("/var/tmp/aesdsocketdata");

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

        connection_fd = accept(socket_fd, (struct sockaddr *) &connection_addr, &connection_addr_len);
        if (connection_fd<0){
            fprintf(stderr, "accept connection error: %s\n", strerror(errno));
            break;
        }

        char peerIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(connection_addr.sin_addr), peerIP, INET_ADDRSTRLEN);

        printf("Accepted connection from %s\n", peerIP);
        syslog(LOG_INFO, "Accepted connection from %s\n", peerIP);

        fp = fopen( "/var/tmp/aesdsocketdata" , "a" );
        if (fp==NULL){
            fprintf(stderr, "open file failed: %s\n", strerror(errno));
            break;
        }

        char read_buffer[256];
        ssize_t bytes_read = 0;
        while (1){
            bytes_read = recv(connection_fd, read_buffer, sizeof(read_buffer)-1, 0);
            fwrite(read_buffer , sizeof(char) , bytes_read , fp );

            // FIXME? we assume \n only appears at the last char of the read
            if (read_buffer[bytes_read-1] == '\n') break;
        }
        fclose(fp); fp = NULL;

        // echo all contents received so far back to client
        fp = fopen( "/var/tmp/aesdsocketdata" , "r" );
        while (1) {
            bytes_read = fread(read_buffer, sizeof(char), sizeof(read_buffer), fp);
            if (bytes_read == 0) break;

            send(connection_fd, read_buffer, bytes_read, 0);
        };

        fclose(fp); fp = NULL;
        close(connection_fd); connection_fd = -1;

        printf("Closed connection from %s\n", peerIP);
        syslog(LOG_INFO, "Closed connection from %s\n", peerIP);


    }

    if (interrupted){
        printf("Caught signal, exiting\n");
        syslog(LOG_INFO, "Caught signal, exiting\n");
    }
    else{

        printf("Exiting due to other errors\n");
    }

    if (fp) fclose(fp);
    if (connection_fd>=0) close(connection_fd);
    if (    socket_fd>=0) close(    socket_fd);
    remove("/var/tmp/aesdsocketdata");

    return 0;
}