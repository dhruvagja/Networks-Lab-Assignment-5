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
#include <time.h>
#include <sys/sem.h>
#include <sys/shm.h>


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


#define wait_sem(s) semop(s, &pop, 1)    
#define signal_sem(s) semop(s, &vop, 1) 

struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};

void* receiver(void *arg){
    fd_set readfds;

    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    key_t keym = ftok(".", 'm');
    int mutex = semget(keym, 1, 0777|IPC_CREAT);

    int latest_seq_no = -1;
    
    while(1){
        
        FD_ZERO(&readfds);
        int max_fd = 0;
        wait_sem(mutex);
        for(int i=0; i<N; i++){
            if(SM[i].free == 0){
                FD_SET(SM[i].udp_sockfd, &readfds);
                // printf("setting fd : %d\n", SM[i].udp_sockfd);
                if(SM[i].udp_sockfd > max_fd){
                    max_fd = SM[i].udp_sockfd;
                }
            }
        }
        signal_sem(mutex);

        int nospace = -1;

        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int ret = select(max_fd+1, &readfds, NULL, NULL, &timeout);
        if(ret == 0){
            // perror("select() timeout");
            if(nospace == 1){
                /*  */
            }
            continue;
        }
        else{
        
            for(int i=0; i<N; i++){
                // wait_sem(mutex);
                if(SM[i].free == 0 && FD_ISSET(SM[i].udp_sockfd, &readfds)){
                    printf("i = %d is set in select\n", i);
                    // if(SM[i].free == 0){
                    // receive
                    // printf("Receiving on socket : %d, and MTP socket: \n", SM[i].udp_sockfd, i);
                    struct sockaddr_in other_addr;
                    socklen_t len = sizeof(other_addr);
                    char buffer[MAXLINE];
                    memset(buffer, 0, sizeof(buffer));
                    int n = recvfrom(SM[i].udp_sockfd, buffer, MAXLINE, 0, (struct sockaddr*)&other_addr, &len);
                    printf("Received buffer after recvfrom = %s\n", buffer);

                    //
                    if(strncmp(buffer, "ACK", 3) == 0){

                        char num[2];
                        memset(num, 0, sizeof(num));
                        num[0] = buffer[4];
                        num[1] = '\0';

                        int num1 = atoi(num);
                        // Set empty after getting ACK
                        SM[i].send_buffer_empty[SM[i].swnd.sequence_numbers[0]] = 1;
                        // char buff[50];
                        // memset(buff, 0, sizeof(buff));
                        // recvfrom(SM[i].udp_sockfd, buff, sizeof(buff), MSG_DONTWAIT , (struct sockaddr*)&other_addr, &len);
                        printf("ACK received: %s\n", buffer);

                        // if timeout we break and resend the message

                        // after ack is received, set the buffer empty
                        
                        // SM[i].sent_unack[SM[i].swnd.sequence_numbers[0]] = 0;
                        // SM[i].send_buffer_empty[SM[i].swnd.sequence_numbers[0]] = 1;
                        // memset(SM[i].send_buffer[SM[i].swnd.sequence_numbers[0]], 0, sizeof(SM[i].send_buffer[SM[i].swnd.sequence_numbers[0]]));
                        
                        SM[i].sent_unack[num1] = 0;
                        SM[i].send_buffer_empty[num1] = 1;
                        memset(SM[i].send_buffer[num1], 0, sizeof(SM[i].send_buffer[num1]));
                        
                        SM[i].swnd.size++;
                        // int last = SM[i].swnd.sequence_numbers[0];
                        // for(int j=0; j<SM[i].swnd.size; j++){
                        //     SM[i].swnd.sequence_numbers[j] = SM[i].swnd.sequence_numbers[j+1];
                        // }
                        // SM[i].swnd.sequence_numbers[2*MAX_WINDOW_SIZE-1] = last;
                        // signal_sem(mutex);
                        printf("sender window size after ACK %d = %d\n", num1, SM[i].swnd.size);
                        continue;
                    }

                    if(n == -1){
                        // signal_sem(mutex);
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

                        // Remove MTP header code left
                        // code goes here  
                    
                        int seq_no = -1;
                        // printing received message
                        // printf("recv: %s\n", buffer);
                        // printf("In receiver thread : received message\n");
                        int num1;
                        for(int j=0; j<SM[i].rwnd.size; j++){
                            if(SM[i].recv_buffer_empty[SM[i].rwnd.sequence_numbers[j]] == 1){
                                printf("Seq Num = %d\n", SM[i].rwnd.sequence_numbers[j]);
                                nospace = 0;
                                seq_no = SM[i].rwnd.sequence_numbers[j];
                                latest_seq_no = SM[i].rwnd.sequence_numbers[j];
                                SM[i].recv_buffer_empty[seq_no] = 0;
                                char msg[MAXLINE];
                                memset(msg, 0, sizeof(msg));
                                char num[2];
                                memset(num, 0, sizeof(num));
                                for(int k=0; k<2; k++){
                                    num[k] = buffer[k+4];
                                }
                                num1 = atoi(num);
                                for(int k=0; k<strlen(buffer); k++){
                                    msg[k] = buffer[k+6];
                                }
                                msg[strlen(buffer)-6] = '\0';
                                strcpy(SM[i].recv_buffer[seq_no], msg);
                                // if(SM[i].rwnd.size == 5){
                                //     printf("Receiver window full\n");
                                //     nospace = 1;
                                //     break;
                                // }
                                SM[i].rwnd.size--;

                                printf("Receiver window size = %d\n", SM[i].rwnd.size);
                                // signal_sem(mutex);
                                break;
                            }
                        }

                        if(nospace == -1) nospace = 1;
                        else{
                            // there was space in the buffer and message was received
                            // send ACK
                            char ACK[50];
                            memset(ACK, 0, sizeof(ACK));
                            
                            sprintf(ACK, "ACK %d, rwnd size = %d\0", num1, SM[i].rwnd.size);
                            printf("In receiver thread : ACK = %s\n", ACK);
                            sendto(SM[i].udp_sockfd, ACK, strlen(ACK), 0, (struct sockaddr*)&other_addr, len);
                            sleep(1);
                        }

                    }

                }
                // signal_sem(mutex);
            }
            
        }

    }

    
}

void* sender(void *arg){
    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    key_t keym = ftok(".", 'm');
    int mutex = semget(keym, 1, 0777|IPC_CREAT);

    // Add MTP header to the message
    while(1){
        // sleep for some time < T/2
        sleep(T/3);

        for(int i = 0; i< N; i++){
            wait_sem(mutex);
            if(SM[i].free == 0){
                // for(int j=0; j<N; j++){
                if(SM[i].send_buffer_empty[SM[i].swnd.sequence_numbers[0]] == 0){
                    // send the message
                    struct sockaddr_in other_addr;
                    other_addr.sin_family = AF_INET;
                    other_addr.sin_port = htons(SM[i].port_other);
                    other_addr.sin_addr.s_addr = inet_addr(SM[i].ip_other);

                    printf("Sending to : %s, %d\n", SM[i].ip_other, SM[i].port_other);
                    
                    socklen_t len = sizeof(other_addr);
                    for(int k=0; k<MAX_WINDOW_SIZE; k++){
                        printf("%d, ", SM[i].swnd.sequence_numbers[k]);
                    }
                    printf("\n");
                    char buffer[MAXLINE+10];
                    memset(buffer, 0, sizeof(buffer));
                    sprintf(buffer, "MTP %d %s", SM[i].swnd.sequence_numbers[0], SM[i].send_buffer[SM[i].swnd.sequence_numbers[0]]);
                    // sendto(SM[i].udp_sockfd, SM[i].send_buffer[SM[i].swnd.sequence_numbers[0]], strlen(SM[i].send_buffer[SM[i].swnd.sequence_numbers[0]]), 0, (struct sockaddr*)&other_addr, len);
                    sendto(SM[i].udp_sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&other_addr, len);
                    printf("sendto: %s\n", SM[i].send_buffer[SM[i].swnd.sequence_numbers[0]]);
                    SM[i].sent_unack[SM[i].swnd.sequence_numbers[0]] = 1;

                    int last = SM[i].swnd.sequence_numbers[0];
                    for(int j=0; j<2*MAX_WINDOW_SIZE; j++){
                        SM[i].swnd.sequence_numbers[j] = SM[i].swnd.sequence_numbers[j+1];
                    }
                    SM[i].swnd.sequence_numbers[2*MAX_WINDOW_SIZE-1] = last;

                    // // Set empty after getting ACK
                    // SM[i].send_buffer_empty[SM[i].swnd.sequence_numbers[0]] = 1;
                    // char buff[50];
                    // memset(buff, 0, sizeof(buff));
                    // recvfrom(SM[i].udp_sockfd, buff, sizeof(buff), MSG_DONTWAIT , (struct sockaddr*)&other_addr, &len);
                    // printf("recvfrom: %s\n", buff);

                    // // if timeout we break and resend the message

                    // // after ack is received, set the buffer empty
                    // if(strncmp(buff, "ACK", 3) == 0){
                    //     SM[i].sent_unack[SM[i].swnd.sequence_numbers[0]] = 0;
                    //     SM[i].send_buffer_empty[SM[i].swnd.sequence_numbers[0]] = 1;
                    //     memset(SM[i].send_buffer[SM[i].swnd.sequence_numbers[0]], 0, sizeof(SM[i].send_buffer[SM[i].swnd.sequence_numbers[0]]));
                    //     SM[i].swnd.size++;
                    //     int last = SM[i].swnd.sequence_numbers[0];
                    //     for(int j=0; j<SM[i].swnd.size; j++){
                    //         SM[i].swnd.sequence_numbers[j] = SM[i].swnd.sequence_numbers[j+1];
                    //     }
                    //     SM[i].swnd.sequence_numbers[2*MAX_WINDOW_SIZE-1] = last;
                    // }
                }
                /*
                for(int j = 0; j<2*MAX_WINDOW_SIZE; j++){
                    if(SM[i].send_buffer_empty[j] == 0 && SM[i].sent_unack[j] == 0){
                        printf("Non empty send buffer = %d\n", i);
                        // send the message
                        struct sockaddr_in other_addr;
                        other_addr.sin_family = AF_INET;
                        other_addr.sin_port = htons(SM[i].port_other);
                        other_addr.sin_addr.s_addr = inet_addr(SM[i].ip_other);

                        printf("Sending to : %s, %d\n", SM[i].ip_other, SM[i].port_other);
                        
                        socklen_t len = sizeof(other_addr);
                        sendto(SM[i].udp_sockfd, SM[i].send_buffer[j], strlen(SM[i].send_buffer[j]), 0, (struct sockaddr*)&other_addr, len);
                        
                        printf("sendto: %s\n", SM[i].send_buffer[j]);
                        SM[i].sent_unack[j] = 1;

                        // Set empty after getting ACK
                        SM[i].send_buffer_empty[j] = 1;
                        char buff[50];
                        memset(buff, 0, sizeof(buff));
                        recvfrom(SM[i].udp_sockfd, buff, sizeof(buff), 0 , (struct sockaddr*)&other_addr, &len);
                        printf("recvfrom: %s\n", buff);

                        // after ack is received, set the buffer empty
                        if(strncmp(buff, "ACK", 3) == 0){
                            SM[i].sent_unack[j] = 0;
                            SM[i].send_buffer_empty[j] = 1;
                            memset(SM[i].send_buffer[j], 0, sizeof(SM[i].send_buffer[j]));


                        }
                    }
                }
                */
            }
            signal_sem(mutex);
        }

        
    }   
}

void* garbage_collector(void *arg){
    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    key_t keym = ftok(".", 'm');
    int mutex = semget(keym, 1, 0777|IPC_CREAT);

    
    while(1){
        for(int i=0; i<N; i++){
            wait_sem(mutex);
            if(SM[i].free == 0){
                int status;
                // flag hata dena
                int ret = waitpid(SM[i].pid, &status, 0);
                // printf("ret = %d, strerror(errno) = %s\n", ret, strerror(errno));
                // printf("ret = %d\n", ret);
                if(ret == SM[i].pid){
                    // process has been killed
                    printf("closing sockfd : %d\n", SM[i].udp_sockfd);  
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
                    SM[i].swnd.size = 10;
                    SM[i].rwnd.size = 5;
                    // memset(SM[i].swnd.sequence_numbers, 0, sizeof(SM[i].swnd.sequence_numbers));
                    // memset(SM[i].rwnd.sequence_numbers, 0, sizeof(SM[i].rwnd.sequence_numbers));
                    for(int j=0; j<MAX_WINDOW_SIZE*2; j++){
                        SM[i].swnd.sequence_numbers[j] = j;         // max window size = 10 and buffer size also 10? change it in future
                    }
                    for(int j=0; j<MAX_WINDOW_SIZE; j++){
                        SM[i].rwnd.sequence_numbers[j] = j;
                    }
                }
            }
            signal_sem(mutex);
        }
    }
}

int main(){

    // creating SM shared memory and initializing it
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);


    key_t keym = ftok(".", 'm');
    int mutex = semget(keym, 1, 0777|IPC_CREAT);
    semctl(mutex, 0, SETVAL, 1);

    wait_sem(mutex);
    for(int i=0; i<N; i++){
        SM[i].free = 1;
        SM[i].pid = 0;
        SM[i].udp_sockfd = 0;
        SM[i].port_other = 0;
        memset(SM[i].ip_other, 0, sizeof(SM[i].ip_other));
        memset(SM[i].send_buffer, 0, sizeof(SM[i].send_buffer));
        memset(SM[i].recv_buffer, 0, sizeof(SM[i].recv_buffer));
        // memset(SM[i].send_buffer_empty, 1, sizeof(SM[i].send_buffer_empty));  // if empty, 1 else 0
        // memset(SM[i].recv_buffer_empty, 1, sizeof(SM[i].recv_buffer_empty));
        for(int j = 0; j<MAX_WINDOW_SIZE*2; j++){
            SM[i].send_buffer_empty[j] = 1;
            SM[i].sent_unack[j] = 0;
        }
        for(int j = 0; j<MAX_WINDOW_SIZE; j++){
            SM[i].recv_buffer_empty[j] = 1;
        }
        // memset(SM[i].sent_unack, 0, sizeof(SM[i].sent_unack));
        // memset(SM[i].swnd.sequence_numbers, 0, sizeof(SM[i].swnd.sequence_numbers));
        // memset(SM[i].rwnd.sequence_numbers, 0, sizeof(SM[i].rwnd.sequence_numbers));
        SM[i].swnd.size = 10;
        SM[i].rwnd.size = 5;
        for(int j=0; j<MAX_WINDOW_SIZE*2; j++){
            SM[i].swnd.sequence_numbers[j] = j;         // max window size = 10 and buffer size also 10? change it in future
        }
        for(int j=0; j<MAX_WINDOW_SIZE; j++){
            SM[i].rwnd.sequence_numbers[j] = j;
        }
    }
    signal_sem(mutex);
    // initializing SOCK_INFO shared memory
    key_t key = ftok(".", 'a');
    int shmid = shmget(key, sizeof(sockinfo), 0777|IPC_CREAT);
    sockinfo *SOCK_INFO = (sockinfo*)shmat(shmid, 0, 0);

    SOCK_INFO->sockfd = 0;
    SOCK_INFO->err = 0;
    SOCK_INFO->port = 0;
    memset(SOCK_INFO->ip, 0, sizeof(SOCK_INFO->ip));

    pthread_t R, S, G;
    // int sem1, sem2;
    // init_sem(&sem1, &sem2);

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&S, &attr, sender, NULL);
    pthread_create(&R, &attr, receiver, NULL);
    pthread_create(&G, &attr, garbage_collector, NULL);

    // // create a shared memory structure sockinfo
    // key_t key = ftok(".", 'a');
    // int shmid = shmget(key, sizeof(sockinfo), 0777|IPC_CREAT);
    // // initialize the shared memory
    // sockinfo *SOCK_INFO = (sockinfo*)shmat(shmid, 0, 0);

    // struct sembuf pop, vop;

    key_t key2 = ftok(".", 'c');
    int sem1 = semget(key2, 1, 0777|IPC_CREAT);
    semctl(sem1, 0, SETVAL, 0);

    key_t key3 = ftok(".", 'd');
    int sem2 = semget(key3, 1, 0777|IPC_CREAT);
    semctl(sem2, 0, SETVAL, 0);

    // pop.sem_num = vop.sem_num = 0;
    // pop.sem_flg = vop.sem_flg = 0;
    // pop.sem_op = -1 ; vop.sem_op = 1;

    // create sockets or binds as per sockfd value
    while(1){
        // printf("Waiting for signal on sem1 : %d\n", sem1);
        wait_sem(sem1);
        // printf("Signal received on sem1\n");
        int sockfd = SOCK_INFO->sockfd;
        int port = SOCK_INFO->port;
        char *ip = SOCK_INFO->ip;
        // socket creation call
        // if(sockfd == 0 && port == 0 && ip[0] == '\0'){
        // printf("Init : sockfd = %d, port = %d, ip = %s\n", sockfd, port, ip);

        if(sockfd > 0 && port == -1){
            // printf("Init : sockfd > 0, port = -2\n");
            // close the socket
            close(sockfd);
            printf("closing sockfd : %d\n", sockfd);
            signal_sem(sem2);
        }
        else if(sockfd == 0 && port == 0){
            // printf("Init : All fields are empty\n");
            int temp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            SOCK_INFO->sockfd = temp_sockfd;
            if(SOCK_INFO->sockfd == -1){
                printf("Socket creation failed : %d\n", errno);
                SOCK_INFO->err = errno;
                // return -1;
            }
            signal_sem(sem2);
        }

        // bind call
        else{
            struct sockaddr_in my_addr;
            my_addr.sin_family = AF_INET;
            my_addr.sin_port = htons(SOCK_INFO->port);
            my_addr.sin_addr.s_addr = inet_addr(SOCK_INFO->ip);
            if(bind(SOCK_INFO->sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1){
                SOCK_INFO->err = errno;
                printf("Bind failed : %d\n", errno);
                SOCK_INFO->sockfd = -1;
                // signal_sem(&sem2);
                // return -1;
            }
            signal_sem(sem2);
            // return -1;
        }
    }

}