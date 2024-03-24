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

#define wait_sem(s) semop(s, &pop1, 1)    
#define signal_sem(s) semop(s, &vop1, 1) 

struct sembuf pop1 = {0, -1, 0}, vop1 = {0, 1, 0};
int transmission_count = 0;

int isTimeout(time_t lastSentTime) {
    time_t currentTime;
    time(&currentTime);
    double timeDiff = difftime(currentTime, lastSentTime);
    // printf("timeDiff = %lf, currentTime = %s, lastSentTime = %s\n", timeDiff, ctime(&currentTime), ctime(&lastSentTime));
    return timeDiff >= T;
}

void* receiver(void *arg){
    fd_set readfds;

    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    key_t keym = ftok(".", 'm');
    int mutex = semget(keym, 1, 0777|IPC_CREAT);

    int latest_seq_no[N];
    for(int i = 0; i<N; i++){
        latest_seq_no[i] = -1;
    }

    int nospace[N];
    for(int i=0; i<N; i++){
        nospace[i] = -1;
    }

    int count = 0;
    int init[N];
    for(int i = 0; i<N; i++){
        init[i] = 0;
    }
    
    while(1){
        
        FD_ZERO(&readfds);
        int max_fd = 0;
        wait_sem(mutex);
        for(int i=0; i<N; i++){
            if(SM[i].free == 0){
                FD_SET(SM[i].udp_sockfd, &readfds);
                if(SM[i].udp_sockfd > max_fd){
                    max_fd = SM[i].udp_sockfd;
                }
            }
        }
        signal_sem(mutex);
        

        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int ret = select(max_fd+1, &readfds, NULL, NULL, &timeout);

        if(ret == 0){
            wait_sem(mutex);
            // perror("select() timeout");
            printf("Timeout in select()\n");
            for(int i = 0; i<N; i++){
                if(nospace[i] == 1 && SM[i].free == 0 && SM[i].rwnd.size > 0){

                    struct sockaddr_in other_addr;
                    other_addr.sin_family = AF_INET;
                    other_addr.sin_port = htons(SM[i].port_other);
                    other_addr.sin_addr.s_addr = inet_addr(SM[i].ip_other);

                    char ACK[50];
                    memset(ACK, 0, sizeof(ACK));
                    
                    sprintf(ACK, "ACK %d, rwnd size = %d", latest_seq_no[i], SM[i].rwnd.size);
                    // printf("In receiver thread on timeout: ACK = %s\n", ACK);
                    sendto(SM[i].udp_sockfd, ACK, strlen(ACK), 0, (struct sockaddr*)&other_addr, sizeof(other_addr));
                    // sleep(1);
                }
            }
            signal_sem(mutex);
            continue;
        }
        else{
        
            for(int i=0; i<N; i++){
                wait_sem(mutex);
                if(SM[i].free == 0 && FD_ISSET(SM[i].udp_sockfd, &readfds)){
                    
                    struct sockaddr_in other_addr;
                    socklen_t len = sizeof(other_addr);
                    char buffer[MAXLINE+10];
                    memset(buffer, 0, sizeof(buffer));
                    int n;
                    
                    n = recvfrom(SM[i].udp_sockfd, buffer, MAXLINE+10, 0, (struct sockaddr*)&other_addr, &len);
                    // printf("Received message %c, %c\n", buffer[0], buffer[4]);
                    if(dropMessage(p)){
                        FILE *fptr = fopen("dropped.txt", "a");
                        // printf("Dropped message %s from MTP %d\n", buffer, i);
                        fprintf(fptr, "Dropped message %s from MTP %d\n", buffer, i);
                        fclose(fptr);
                        signal_sem(mutex);
                        continue;
                    }
                    

                
                    if(strncmp(buffer, "ACK", 3) == 0){

                        char num[2];
                        memset(num, 0, sizeof(num));
                        num[0] = buffer[4];
                        num[1] = '\0';

                        int num1 = atoi(num);
                        if(num[0] == '-'){
                            signal_sem(mutex);
                            continue;
                        }
                        
                        // printf("ACK received for %d\n", num1);
                        if(SM[i].sent_unack[num1]) SM[i].sent_unack[num1] = 0;
                        else{
                            // printf("duplicate ack received\n");
                            for(int j=0; j<2*MAX_WINDOW_SIZE; j++){
                                if(SM[i].send_buffer_empty[j] == 0 && SM[i].sent_unack[j] == 1){
                                    SM[i].sent_unack[j] = 0;
                                    SM[i].swnd.size++;
                                }
                            }
                            for(int j = 0; j< 2*MAX_WINDOW_SIZE; j++){
                                SM[i].swnd.sequence_numbers[j] = (num1 + 1 + j)%(2*MAX_WINDOW_SIZE);
                            }

                            signal_sem(mutex);
                            continue;

                        }
                        SM[i].send_buffer_empty[num1] = 1;
                        memset(SM[i].send_buffer[num1], 0, sizeof(SM[i].send_buffer[num1]));

                        SM[i].swnd.size++;
                       
                        signal_sem(mutex);
                        continue;
                    }

                    char seq[2];
                    memset(seq, 0, sizeof(seq));
                    seq[0] = buffer[4];
                    seq[1] = '\0';

                    int seq1 = atoi(seq);

                    
                    if((seq1 != (latest_seq_no[i]+1)%10)){
                        char ACK[50];
                        memset(ACK, 0, sizeof(ACK));
                        int temp = latest_seq_no[i];
                        sprintf(ACK, "ACK %d, rwnd size = %d", temp, SM[i].rwnd.size);
                        sendto(SM[i].udp_sockfd, ACK, strlen(ACK), 0, (struct sockaddr*)&other_addr, len);
                        signal_sem(mutex);
                        continue;
                    }

                    if(n == -1){
                        signal_sem(mutex);
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            
                            continue;
                        }
                        else{

                            perror("recvfrom() failed");
                            exit(1);
                        }
                    }
                    else{
                        int seq_no = -1;
                        
                        int num1;
                        int flag = 0;
                        for(int j=0; j<MAX_WINDOW_SIZE; j++){
                            if(SM[i].recv_buffer_empty[SM[i].rwnd.sequence_numbers[j]] == 1){
                                if(init[i] == 0){
                                    init[i] = 1;
                                }
                                nospace[i] = 0;
                                flag = 1;
                                seq_no = SM[i].rwnd.sequence_numbers[j];
                                
                            
                                SM[i].recv_buffer_empty[seq_no] = 0;
                                char msg[MAXLINE];
                                memset(msg, 0, sizeof(msg));
                                char num[2];
                                memset(num, 0, sizeof(num));
                                for(int k=0; k<1; k++){
                                    num[k] = buffer[k+4];
                                }
                                num1 = atoi(num);
                                for(int k=6; k<strlen(buffer); k++){
                                    msg[k-6] = buffer[k];
                                }
                                strcpy(SM[i].recv_buffer[seq_no], msg);
                               
                                SM[i].rwnd.size--;

                                break;
                            }
                        }
                        
                        if(flag == 0) nospace[i] = 1;
                        else{
                            
                            latest_seq_no[i] = num1;

                            char ACK[50];
                            memset(ACK, 0, sizeof(ACK));
                            
                            sprintf(ACK, "ACK %d, rwnd size = %d", num1, SM[i].rwnd.size);
                            // printf("In receiver thread : ACK = %s\n", ACK);
                            sendto(SM[i].udp_sockfd, ACK, strlen(ACK), 0, (struct sockaddr*)&other_addr, len);
                            
                            // sleep(1);
                        }
                        

                    }

                }
                signal_sem(mutex);
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

    time_t lastSentTime[N][MAX_WINDOW_SIZE*2];
    
    
    // Add MTP header to the message
    while(1){
        // sleep for some time < T/2
        sleep((T)/5);

        for(int i = 0; i< N; i++){
            wait_sem(mutex);
            if(SM[i].free == 0 ){

                for(int j=0; j<2*MAX_WINDOW_SIZE; j++){
                    if(SM[i].sent_unack[j] == 1 && isTimeout(lastSentTime[i][j])){
                        for(int k=0; k<2*MAX_WINDOW_SIZE; k++){
                            if(SM[i].send_buffer_empty[k] == 0 && SM[i].sent_unack[k] == 1){
                                SM[i].sent_unack[k] = 0;
                                SM[i].swnd.size++;
                            }
                        }
                        for(int k = 0; k< 2*MAX_WINDOW_SIZE; k++){
                            SM[i].swnd.sequence_numbers[k] = (j + k)%(2*MAX_WINDOW_SIZE);
                        }

                    }
                }

                if(SM[i].send_buffer_empty[SM[i].swnd.sequence_numbers[0]] == 0 && SM[i].sent_unack[SM[i].swnd.sequence_numbers[0]] == 0){
                   
                    struct sockaddr_in other_addr;
                    other_addr.sin_family = AF_INET;
                    other_addr.sin_port = htons(SM[i].port_other);
                    other_addr.sin_addr.s_addr = inet_addr(SM[i].ip_other);

                    socklen_t len = sizeof(other_addr);
                   
                    char buffer[MAXLINE+10];
                    memset(buffer, 0, sizeof(buffer));
                    sprintf(buffer, "MTP %d %s", SM[i].swnd.sequence_numbers[0], SM[i].send_buffer[SM[i].swnd.sequence_numbers[0]]);
                    if(SM[i].swnd.size > 0) {
                        sendto(SM[i].udp_sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&other_addr, len);
                        transmission_count++;
                    }
                    else{
                        printf("Sender window is empty, maybe recv buffer is full\n");
                        signal_sem(mutex);
                        continue;
                    }
                    time(&lastSentTime[i][SM[i].swnd.sequence_numbers[0]]);
                    SM[i].sent_unack[SM[i].swnd.sequence_numbers[0]] = 1;

                    int last = SM[i].swnd.sequence_numbers[0];
                    for(int j=0; j<2*MAX_WINDOW_SIZE; j++){
                        SM[i].swnd.sequence_numbers[j] = SM[i].swnd.sequence_numbers[j+1];
                    }
                    SM[i].swnd.sequence_numbers[2*MAX_WINDOW_SIZE-1] = last;
                    SM[i].swnd.size--;
                    
                }
                
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
                    SM[i].swnd.size = 5;
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
        for(int j=0; j<MAX_WINDOW_SIZE*2; j++){
            memset(SM[i].send_buffer[j], 0, sizeof(SM[i].send_buffer[j]));
            if(j < MAX_WINDOW_SIZE)memset(SM[i].recv_buffer[j], 0, sizeof(SM[i].recv_buffer[j]));
        }
        // memset(SM[i].send_buffer, 0, sizeof(SM[i].send_buffer));
        // memset(SM[i].recv_buffer, 0, sizeof(SM[i].recv_buffer));
        // memset(SM[i].send_buffer_empty, 1, sizeof(SM[i].send_buffer_empty));  // if empty, 1 else 0
        // memset(SM[i].recv_buffer_empty, 1, sizeof(SM[i].recv_buffer_empty));
        for(int j = 0; j<MAX_WINDOW_SIZE*2; j++){
            SM[i].send_buffer_empty[j] = 1;
            SM[i].sent_unack[j] = 0;
        }
        for(int j = 0; j<MAX_WINDOW_SIZE; j++){
            SM[i].recv_buffer_empty[j] = 1;
        }
        SM[i].swnd.size = 5;
        SM[i].rwnd.size = 5;
        for(int j=0; j<MAX_WINDOW_SIZE*2; j++){
            SM[i].swnd.sequence_numbers[j] = j;         // max window size = 10 and buffer size also 10? change it in future
        }
        for(int j=0; j<MAX_WINDOW_SIZE; j++){
            SM[i].rwnd.sequence_numbers[j] = j;
        }
    }
    signal_sem(mutex);
    key_t key = ftok(".", 'a');
    int shmid = shmget(key, sizeof(sockinfo), 0777|IPC_CREAT);
    sockinfo *SOCK_INFO = (sockinfo*)shmat(shmid, 0, 0);

    SOCK_INFO->sockfd = 0;
    SOCK_INFO->err = 0;
    SOCK_INFO->port = 0;
    memset(SOCK_INFO->ip, 0, sizeof(SOCK_INFO->ip));

    pthread_t R, S, G;

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&S, &attr, sender, NULL);
    pthread_create(&R, &attr, receiver, NULL);
    pthread_create(&G, &attr, garbage_collector, NULL);

   

    key_t key2 = ftok(".", 'c');
    int sem1 = semget(key2, 1, 0777|IPC_CREAT);
    semctl(sem1, 0, SETVAL, 0);

    key_t key3 = ftok(".", 'd');
    int sem2 = semget(key3, 1, 0777|IPC_CREAT);
    semctl(sem2, 0, SETVAL, 0);

    
    while(1){
        // printf("Waiting for signal on sem1 : %d\n", sem1);
        wait_sem(sem1);
        // printf("Signal received on sem1\n");
        int sockfd = SOCK_INFO->sockfd;
        int port = SOCK_INFO->port;
        char *ip = SOCK_INFO->ip;
        

        if(sockfd > 0 && port == -1){
            // printf("Init : sockfd > 0, port = -2\n");
            // close the socket
            close(sockfd);
            printf("Transmission count = %d\n", transmission_count);
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