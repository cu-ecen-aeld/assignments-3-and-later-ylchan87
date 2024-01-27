#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <sys/queue.h>


struct Per_connection_data{
    /*
     * TODO: add other values your thread will need to manage
     * into this structure, use this structure to communicate
     * between the start_thread_obtaining_mutex function and
     * your thread implementation.
     */

    /**
     * Set to true if the thread completed with success, false
     * if an error occurred.
     */
    bool thread_complete_success;
    int  pid;

    FILE* fp;
    pthread_mutex_t *file_use_mutex;

    int connection_fd;
    char peerIP[INET_ADDRSTRLEN];
};

struct SList_connection {
    pthread_t thread;
    struct Per_connection_data* data;
    SLIST_ENTRY(SList_connection) entries;
};