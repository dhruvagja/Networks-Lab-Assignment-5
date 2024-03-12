#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <errno.h>
#include <sys/time.h>
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
    fd_set readfds;
    while(1){
        
        FD_ZERO(&readfds);
        
        for(int i=0; i<N; i++){
            if(SM[i].free == 0){
                FD_SET(SM[i].udp_sockfd, &readfds);
            }
        }

        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int ret = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
        if(ret == 0){

            continue;
        }
        else{
            for(int i=0; i<N; i++){
                if(SM[i].free == 0 && FD_ISSET(SM[i].udp_sockfd, &readfds)){
                    // receive
                    struct sockaddr_in other_addr;
                    int len = sizeof(other_addr);
                    int n = recvfrom(SM[i].udp_sockfd, SM[i].recv_buffer[SM[i].rwnd.size], MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&other_addr, &len);
                    if(n == -1){
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            continue;
                        }
                        else{
                            perror("recvfrom() failed");
                            exit(1);
                        }
                    }
                    else{
                        // SM[i].recv_buffer_empty[SM[i].rwnd.size] = 0;
                        // SM[i].rwnd.sequence_numbers[SM[i].rwnd.size] = SM[i].rwnd.size;
                        // SM[i].rwnd.size++;
                        
                    }
                }
            }
            
        }

    }
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