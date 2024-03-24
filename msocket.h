#ifndef MSOCKET_H
#define MSOCKET_H

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <errno.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <time.h>
#define T 5
#define p 0.7
#define SOCK_MTP 0

#define MAXLINE 1024
#define MAX_WINDOW_SIZE 5
#define N 25

// #define wait_sem(s) semop(s, &pop, 1)    
// #define signal_sem(s) semop(s, &vop, 1)  

// struct sembuf pop, vop;

typedef struct {
    int size;
    int sequence_numbers[MAX_WINDOW_SIZE*2];
}sender_window;

typedef struct {
    int size;
    int sequence_numbers[MAX_WINDOW_SIZE];
}receiver_window;

typedef struct {
    int free;
    int pid;
    int udp_sockfd;
    char ip_other[16];
    int port_other;
    char send_buffer[MAX_WINDOW_SIZE*2][MAXLINE];
    int send_buffer_empty[MAX_WINDOW_SIZE*2];
    int sent_unack[MAX_WINDOW_SIZE*2];
    char recv_buffer[MAX_WINDOW_SIZE][MAXLINE];
    int recv_buffer_empty[MAX_WINDOW_SIZE];
    sender_window swnd;
    receiver_window rwnd;
}SM_;

// SM_ SM[N];

typedef struct {
    int sockfd;
    char ip[16];
    int port;
    int err;
}sockinfo;

// sockinfo SOCK_INFO = {0, "", 0, 0};

void reset();

// create two semaphores sem1 and sem2


// semctl(sem1, 0, SETVAL, 0);
// semctl(sem2, 0, SETVAL, 1);

// void init_sem();

int m_socket(int domain, int type, int protocol);
// taking ip as string rn, can take differently
int m_bind(int sockid, char *source_ip, int source_port, char *dest_ip, int dest_port);
ssize_t m_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
ssize_t m_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len);
int m_close(int socket);
int dropMessage(float prob);


#endif 