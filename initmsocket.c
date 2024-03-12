#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <errno.h>

#include <pthread.h>
#include "msocket.h"

// #define MAXLINE 1024
// #define MAX_WINDOW_SIZE 5
// #define N 25

// typedef struct {
//     int size;
//     int sequence_numbers[MAX_WINDOW_SIZE*2];
// }sender_window;

// typedef struct {
//     int size;
//     int sequence_numbers[MAX_WINDOW_SIZE];
// }receiver_window;

// typedef struct {
//     int free;
//     int pid;
//     int udp_sockfd;
//     char *ip_other;
//     int port_other;
//     char send_buffer[MAX_WINDOW_SIZE*2][MAXLINE];
//     char recv_buffer[MAX_WINDOW_SIZE][MAXLINE];
//     sender_window swnd;
//     receiver_window rwnd;
// };


void* receiver(void *arg){

}

void* sender(void *arg){

}

void* garbage_collector(void *arg){

}

int main(){
    pthread_t R, S, G;

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&R, &attr, receiver, NULL);
    pthread_create(&S, &attr, sender, NULL);
    pthread_create(&G, &attr, garbage_collector, NULL);
}