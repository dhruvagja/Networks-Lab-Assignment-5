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

    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    int latest_seq_no = -1;

    while(1){
        
        FD_ZERO(&readfds);
        int max_fd = 0;
        
        for(int i=0; i<N; i++){
            if(SM[i].free == 0){
                FD_SET(SM[i].udp_sockfd, &readfds);
                if(SM[i].udp_sockfd > max_fd){
                    max_fd = SM[i].udp_sockfd;
                }
            }
        }

        int nospace = -1;

        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int ret = select(max_fd+1, &readfds, NULL, NULL, &timeout);
        if(ret == 0){
            if(nospace == 1){
                // char ACK[20];
                // sprintf(ACK, "ACK %d", SM[i].rwnd.sequence_numbers[0]);

                // send latest seq no ACK, with updated rwnd size

            }
            continue;
        }
        else{
            for(int i=0; i<N; i++){
                if(SM[i].free == 0 && FD_ISSET(SM[i].udp_sockfd, &readfds)){
                    // receive
                    struct sockaddr_in other_addr;
                    int len = sizeof(other_addr);
                    char buffer[MAXLINE];
                    int n = recvfrom(SM[i].udp_sockfd, buffer, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&other_addr, &len);
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

                        int seq_no = -1;
                        for(int j=0; j<MAX_WINDOW_SIZE; j++){
                            if(SM[i].recv_buffer_empty[j] == 1){
                                nospace = 0;
                                SM[i].recv_buffer_empty[j] = 0;
                                strcpy(SM[i].recv_buffer[j], buffer);
                                if(SM[i].rwnd.size == 5){
                                    nospace = 1;
                                    break;
                                }
                                seq_no = j;
                                latest_seq_no = j;
                                SM[i].rwnd.size++;
                                break;
                            }
                        }

                        if(nospace == -1) nospace = 1;
                        else{
                            // there was space in the buffer and message was received
                            // send ACK
                            char ACK[50];
                            sprintf(ACK, "ACK %d, rwnd size = %d", seq_no, SM[i].rwnd.size);
                            sendto(SM[i].udp_sockfd, ACK, strlen(ACK), MSG_DONTWAIT, (struct sockaddr*)&other_addr, len);
                        }

                    }

                }
            }
            
        }

    }
}

void* sender(void *arg){
    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    while(1){

    }
}

void* garbage_collector(void *arg){
    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    while(1){
        for(int i=0; i<N; i++){
            if(SM[i].free == 0){
                int status;
                // flag hata dena
                int ret = waitpid(SM[i].pid, &status, WNOHANG);
                if(ret == SM[i].pid){
                    // process has been killed
                    close(SM[i].udp_sockfd);
                    SM[i].free = 1;
                    SM[i].pid = 0;
                    SM[i].udp_sockfd = 0;
                    SM[i].port_other = 0;
                    memset(SM[i].ip_other, 0, sizeof(SM[i].ip_other));
                    memset(SM[i].send_buffer, 0, sizeof(SM[i].send_buffer));
                    memset(SM[i].recv_buffer, 0, sizeof(SM[i].recv_buffer));
                    memset(SM[i].send_buffer_empty, 1, sizeof(SM[i].send_buffer_empty));  // if empty, 1 else 0
                    memset(SM[i].recv_buffer_empty, 1, sizeof(SM[i].recv_buffer_empty));
                    memset(SM[i].swnd.sequence_numbers, 0, sizeof(SM[i].swnd.sequence_numbers));
                    memset(SM[i].rwnd.sequence_numbers, 0, sizeof(SM[i].rwnd.sequence_numbers));
                }
            }
        }
    }

}

int main(){

    // creating SM shared memory and initializing it
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    for(int i=0; i<N; i++){
        SM[i].free = 1;
        SM[i].pid = 0;
        SM[i].udp_sockfd = 0;
        SM[i].port_other = 0;
        memset(SM[i].ip_other, 0, sizeof(SM[i].ip_other));
        memset(SM[i].send_buffer, 0, sizeof(SM[i].send_buffer));
        memset(SM[i].recv_buffer, 0, sizeof(SM[i].recv_buffer));
        memset(SM[i].send_buffer_empty, 1, sizeof(SM[i].send_buffer_empty));  // if empty, 1 else 0
        memset(SM[i].recv_buffer_empty, 1, sizeof(SM[i].recv_buffer_empty));
        // memset(SM[i].swnd.sequence_numbers, 0, sizeof(SM[i].swnd.sequence_numbers));
        // memset(SM[i].rwnd.sequence_numbers, 0, sizeof(SM[i].rwnd.sequence_numbers));
        for(int j=0; j<MAX_WINDOW_SIZE*2; j++){
            SM[i].swnd.sequence_numbers[j] = j;         // max window size = 10 and buffer size also 10? change it in future
        }
        for(int j=0; j<MAX_WINDOW_SIZE; j++){
            SM[i].rwnd.sequence_numbers[j] = j;
        }
    }

    // initializing SOCK_INFO shared memory
    key_t key = ftok(".", 'a');
    int shmid = shmget(key, sizeof(sockinfo), 0777|IPC_CREAT);
    sockinfo *SOCK_INFO = (sockinfo*)shmat(shmid, 0, 0);

    SOCK_INFO->sockfd = 0;
    SOCK_INFO->err = 0;
    SOCK_INFO->port = 0;
    memset(SOCK_INFO->ip, 0, sizeof(SOCK_INFO->ip));

    pthread_t R, S, G;
    init_sem();

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&R, &attr, receiver, NULL);
    pthread_create(&S, &attr, sender, NULL);
    pthread_create(&G, &attr, garbage_collector, NULL);

    // // create a shared memory structure sockinfo
    // key_t key = ftok(".", 'a');
    // int shmid = shmget(key, sizeof(sockinfo), 0777|IPC_CREAT);
    // // initialize the shared memory
    // sockinfo *SOCK_INFO = (sockinfo*)shmat(shmid, 0, 0);

    // create sockets or binds as per sockfd value
    while(1){
        wait_sem(&sem1);
        int sockfd = SOCK_INFO->sockfd;
        int port = SOCK_INFO->port;
        char *ip = SOCK_INFO->ip;
        // socket creation call
        if(sockfd == 0 && port == 0 && strlen(ip) == 0){
            SOCK_INFO->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if(SOCK_INFO->sockfd == -1){
                SOCK_INFO->err = strerror(errno);
                // return -1;
            }
            signal_sem(&sem2);
        }

        // bind call
        else{
            struct sockaddr_in my_addr;
            my_addr.sin_family = AF_INET;
            my_addr.sin_port = htons(SOCK_INFO->port);
            my_addr.sin_addr.s_addr = inet_addr(SOCK_INFO->ip);
            if(bind(SOCK_INFO->sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1){
                SOCK_INFO->err = strerror(errno);
                SOCK_INFO->sockfd = -1;
                signal_sem(&sem2);
                // return -1;
            }
            signal_sem(&sem2);
            // return -1;
        }
    }

}