#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#include "msocket.h"

int sem1, sem2;
struct sembuf pop, vop;

#define wait_sem(s) semop(s, &pop, 1)    
#define signal_sem(s) semop(s, &vop, 1)  

void reset(){
    // get sockinfo shared memory
    key_t key = ftok(".", 'a');
    int shmid = shmget(key, sizeof(sockinfo), 0777|IPC_CREAT);
    // initialize the shared memory
    sockinfo *SOCK_INFO = (sockinfo*)shmat(shmid, 0, 0);

    SOCK_INFO->sockfd = 0;
    SOCK_INFO->err = 0;
    SOCK_INFO->port = 0;
    memset(SOCK_INFO->ip, 0, sizeof(SOCK_INFO->ip));
}

void init_sem(){
    // struct sembuf pop, vop;
    key_t key = ftok(".", 'c');
    key_t key1 = ftok(".", 'd');

    sem1 = semget(key, 1, 0777|IPC_CREAT);
    sem2 = semget(key1, 1, 0777|IPC_CREAT);

    pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1;
}

int m_socket(int domain, int type, int protocol){
    int free_entry = 0;

    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    // get sockinfo shared memory
    key_t key = ftok(".", 'a');
    int shmid = shmget(key, sizeof(sockinfo), 0777|IPC_CREAT);
    // initialize the shared memory
    sockinfo *SOCK_INFO = (sockinfo*)shmat(shmid, 0, 0);

    for(int i=0; i<N; i++){
        if(SM[i].free == 1){
            // free_entry = 1;
            // SM[i].free = 0;
            // SM[i].pid = getpid();
            // SM[i].udp_sockfd = socket(domain, type, protocol);

            // return i;

            free_entry = 1;
            init_sem();
            printf("signaling sem1 = %d\n", sem1);
            signal_sem(sem1);
            wait_sem(sem2);

            if(SOCK_INFO->sockfd == -1){
                errno = SOCK_INFO->err;
                return -1;
            }
            else{

                printf("Waiting done in m_socket\n UDP socket : %d\n", SOCK_INFO->sockfd);
                SM[i].free = 0;
                SM[i].pid = getpid();
                SM[i].udp_sockfd = SOCK_INFO->sockfd;
                printf("UDP socket : %d\n", SM[i].udp_sockfd);
                // memset(SM[i].ip_other, 0, sizeof(SM[i].ip_other));
                SM[i].port_other = 0;   
                // memset(SM[i].send_buffer, 0, sizeof(SM[i].send_buffer));
                // memset(SM[i].recv_buffer, 0, sizeof(SM[i].recv_buffer));
                // memset(SM[i].send_buffer_empty, 0, sizeof(SM[i].send_buffer_empty));
                // memset(SM[i].recv_buffer_empty, 0, sizeof(SM[i].recv_buffer_empty));
                reset();
                // signal_sem(&sem1);
                printf("Returning from m_socket, MTP socket : %d\n", i);
                return i;
            }
        }
    }

    if(!free_entry){
        //  If no free entry is available, it returns -1 with the global error variable set to ENOBUFS.
        errno = ENOBUFS;
        return -1;
    }

    return -1;
}

int m_bind(int sockid, char *source_ip, int source_port, char *dest_ip, int dest_port){

    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    if(SM[sockid].udp_sockfd == -1){
        errno = EBADF;
        return -1;
    }

    // get sockinfo shared memory
    key_t key = ftok(".", 'a');
    int shmid = shmget(key, sizeof(sockinfo), 0777|IPC_CREAT);
    // initialize the shared memory
    sockinfo *SOCK_INFO = (sockinfo*)shmat(shmid, 0, 0);

    SOCK_INFO->sockfd = SM[sockid].udp_sockfd;
    SOCK_INFO->port = source_port;
    strcpy(SOCK_INFO->ip, source_ip);

    init_sem();

    signal_sem(sem1);
    wait_sem(sem2);

    if(SOCK_INFO->sockfd == -1){
        errno = SOCK_INFO->err;
        reset();
        return -1;
    }

    // update the corresponding SM with the destination IP and destination port. 
    SM[sockid].port_other = dest_port;
    strcpy(SM[sockid].ip_other, dest_ip);

    reset();

    return 0;

}


ssize_t m_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len){
    
    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    printf("msocket.c | m_sendto function | before checking valid sockid: send mesg: %s\n", (char*)message);
    printf("destination IP : %s\n", inet_ntoa(((struct sockaddr_in *)dest_addr)->sin_addr));
    // check if the socket is valid
    if(SM[socket].udp_sockfd == -1){
        errno = EBADF;
        return -1;
    }

    // get destination port and ip from sockaddr
    struct sockaddr_in *dest = (struct sockaddr_in *)dest_addr;
    int dest_port = ntohs(dest->sin_port);
    char *dest_ip = inet_ntoa(dest->sin_addr);

    // check if the destination port and ip are same as the one in the SM
    if(strcmp(SM[socket].ip_other, dest_ip) != 0 || SM[socket].port_other != dest_port){
        // ENOTFOUND errno?
        errno = EDESTADDRREQ;
        return -1;
    }

    int isspace = 0;
    char *message_ = (char *)message;
    for(int i=0; i<MAX_WINDOW_SIZE*2; i++){
        printf("Using %d MTP socket\n", socket);
        printf("SM[socket].send_buffer_empty[i] : %d, SM[socket].swnd.sequence[j] = %d \n", SM[socket].send_buffer_empty[i], SM[socket].swnd.sequence_numbers[i]);
        printf("SM[i].free : %d\n", SM[i].free);
        if(SM[socket].send_buffer_empty[i] == 1){   
            isspace = 1;
            SM[socket].send_buffer_empty[i] = 0;
            strcpy(SM[socket].send_buffer[i], message_);
            printf("Typecased message: %s\n", SM[socket].send_buffer[i]);
            SM[socket].swnd.size++;
            return 0;   // if successful
        }
    }

    // if no space is available in the send buffer, it returns -1 with the global error variable set to ENOBUFS.
    if(!isspace){
        errno = ENOBUFS;
        return -1;
    }

    return -1;
    
}

ssize_t m_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len){
    // check if the socket is valid

    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    if(SM[socket].udp_sockfd == -1){
        errno = EBADF;
        return -1;
    }

    // get the source port and ip from the sockaddr
    struct sockaddr_in *src = (struct sockaddr_in *)address;
    int src_port = ntohs(src->sin_port);
    char *src_ip = inet_ntoa(src->sin_addr);

    // check if the source port and ip are same as the one in the SM
    // if(strcmp(SM[socket].ip_other, src_ip) != 0 || SM[socket].port_other != src_port){
    //     // ENOTFOUND errno?

    //     printf("Source port and ip not same %d, %s \n", src_port, src_ip);
    //     errno = EDESTADDRREQ;
    //     return -1;
    // }

    int ismsg = 0;
    // for(int i = 0; i< MAX_WINDOW_SIZE; i++){
    //     if(SM[socket].recv_buffer_empty[i] == 0){
    //         ismsg = 1;
    //         SM[socket].recv_buffer_empty[i] = 1;
    //         strcpy(buffer, SM[socket].recv_buffer[i]);
    //         memset(SM[socket].recv_buffer[i], 0,sizeof(SM[socket].recv_buffer[i]));
    //         // Returning the length of the message received.
    //         return strlen(buffer);
    //     }
    // }

    // char *buffer_ = (char *)buffer;
    // printf("msocket.c | m_recvfrom function | after checking valid sockid: senrecvd mesg: %s\n", buffer_);
    if(SM[socket].recv_buffer_empty[SM[socket].rwnd.sequence_numbers[0]] == 0){
        ismsg = 1;
        SM[socket].recv_buffer_empty[SM[socket].rwnd.sequence_numbers[0]] = 1;
        strcpy((char*)buffer, SM[socket].recv_buffer[SM[socket].rwnd.sequence_numbers[0]]);
        memset(SM[socket].recv_buffer[SM[socket].rwnd.sequence_numbers[0]], 0,sizeof(SM[socket].recv_buffer[SM[socket].rwnd.sequence_numbers[0]]));
        // Returning the length of the message received.
        printf("msocket.c | m_recvfrom function | after checking valid sockid: senrecvd mesg: %s\n", (char*)buffer);
        return strlen((char*)buffer);
    }

    // char buffer = SM[socket].recv_buffer[SM[socket].rwnd.sequence_numbers[0]];

    if(!ismsg){
        errno = ENOBUFS;
        return -1;
    }

    return -1;
}

int m_close(int socket){

    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    close(SM[socket].udp_sockfd);
    SM[socket].free = 1;
    SM[socket].pid = 0;
    SM[socket].udp_sockfd = 0;
    SM[socket].port_other = 0;
    memset(SM[socket].ip_other, 0, sizeof(SM[socket].ip_other));
    memset(SM[socket].send_buffer, 0, sizeof(SM[socket].send_buffer));
    memset(SM[socket].recv_buffer, 0, sizeof(SM[socket].recv_buffer));
    memset(SM[socket].send_buffer_empty, 1, sizeof(SM[socket].send_buffer_empty));  // if empty, 1 else 0
    memset(SM[socket].recv_buffer_empty, 1, sizeof(SM[socket].recv_buffer_empty));
    memset(SM[socket].swnd.sequence_numbers, 0, sizeof(SM[socket].swnd.sequence_numbers));
    memset(SM[socket].rwnd.sequence_numbers, 0, sizeof(SM[socket].rwnd.sequence_numbers));
    memset(SM[socket].sent_unack, 0, sizeof(SM[socket].sent_unack));
    return 0;
}

int dropMessage(float prob){
    srand(time(0)); 
    float r = (float)rand()/(float)(RAND_MAX);
    if(r < prob){
        return 1;
    }
    return 0;
}


