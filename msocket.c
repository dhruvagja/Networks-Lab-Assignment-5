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
int mutex;

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
    key_t keym = ftok(".", 'm');
    mutex = semget(keym, 1, 0777|IPC_CREAT);

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
    init_sem();
    for(int i=0; i<N; i++){
        // wait_sem(mutex);
        if(SM[i].free == 1){
            

            free_entry = 1;
            // printf("signaling sem1 = %d\n", sem1);
            signal_sem(sem1);
            wait_sem(sem2);

            if(SOCK_INFO->sockfd == -1){
                errno = SOCK_INFO->err;
                // signal_sem(mutex);
                return -1;
            }
            else{

                // printf("Waiting done in m_socket\n UDP socket : %d\n", SOCK_INFO->sockfd);
                SM[i].free = 0;
                SM[i].pid = getpid();
                SM[i].udp_sockfd = SOCK_INFO->sockfd;
                // printf("UDP socket : %d\n", SM[i].udp_sockfd);
                // memset(SM[i].ip_other, 0, sizeof(SM[i].ip_other));
                SM[i].port_other = 0;   
                
                reset();
                
                return i;
            }
        }
        // signal_sem(mutex);
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
        printf("Error in binding\n");
        reset();
        return -1;
    }

    // update the corresponding SM with the destination IP and destination port. 
    wait_sem(mutex);
    SM[sockid].port_other = dest_port;
    strcpy(SM[sockid].ip_other, dest_ip);

    signal_sem(mutex);

    reset();

    return 0;

}


ssize_t m_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len){
    
    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);
    init_sem();
    
    
    if(SM[socket].udp_sockfd == -1){
        errno = EBADF;
        printf("Invalid socket\n");
        return -1;
    }

    // get destination port and ip from sockaddr
    struct sockaddr_in *dest = (struct sockaddr_in *)dest_addr;
    int dest_port = ntohs(dest->sin_port);
    char *dest_ip = inet_ntoa(dest->sin_addr);

    
    if(strcmp(SM[socket].ip_other, dest_ip) != 0 || SM[socket].port_other != dest_port){
        errno = ENOTCONN;
        printf("Destination port and ip not same %d, %s , %s, %d\n", dest_port, dest_ip, SM[socket].ip_other, SM[socket].port_other);
        return -1;
    }
    int isspace = 0;
    char *message_ = (char *)message;

    wait_sem(mutex);
    for(int i=0; i<MAX_WINDOW_SIZE*2; i++){
        
        if(SM[socket].send_buffer_empty[SM[socket].swnd.sequence_numbers[i]] == 1){
            // printf("Using socket in m_sendto: %d\n", socket);   
            isspace = 1;
            SM[socket].send_buffer_empty[SM[socket].swnd.sequence_numbers[i]] = 0;
            // printf("seq_no in msocket = %d\n", SM[socket].swnd.sequence_numbers[i]);
            strcpy(SM[socket].send_buffer[SM[socket].swnd.sequence_numbers[i]], message_);
            
            signal_sem(mutex);
            return 0;   // if successful
        }
    }

    signal_sem(mutex);

    if(!isspace){
        errno = ENOBUFS;
        // printf("No space available in send buffer\n");
        return -2;
    }

    return -1;
    
}

ssize_t m_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len){
    // check if the socket is valid

    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);
    init_sem();
    
    if(SM[socket].udp_sockfd == -1){
        errno = EBADF;
        return -1;
    }

    // get the source port and ip from the sockaddr
    struct sockaddr_in *src = (struct sockaddr_in *)address;
    int src_port = ntohs(src->sin_port);
    char *src_ip = inet_ntoa(src->sin_addr);

   
    int ismsg = 0;
    // char *buffer_ = (char *)buffer;
    // printf("msocket.c | m_recvfrom function | after checking valid sockid: senrecvd mesg: %s\n", buffer_);
    wait_sem(mutex);
    
    if(SM[socket].recv_buffer_empty[SM[socket].rwnd.sequence_numbers[0]] == 0){
        // printf("In the if block\n");
        ismsg = 1;
        strcpy((char*)buffer, SM[socket].recv_buffer[SM[socket].rwnd.sequence_numbers[0]]);

        memset(SM[socket].recv_buffer[SM[socket].rwnd.sequence_numbers[0]], 0,sizeof(SM[socket].recv_buffer[SM[socket].rwnd.sequence_numbers[0]]));

        SM[socket].recv_buffer_empty[SM[socket].rwnd.sequence_numbers[0]] = 1;

        
        int last = SM[socket].rwnd.sequence_numbers[0];
        for(int k=0; k<MAX_WINDOW_SIZE; k++){
            SM[socket].rwnd.sequence_numbers[k] = SM[socket].rwnd.sequence_numbers[k+1];
        }
        SM[socket].rwnd.sequence_numbers[MAX_WINDOW_SIZE-1] = last;
        SM[socket].rwnd.size++;
        // printf("Window size = %d\n", SM[socket].rwnd.size);
        signal_sem(mutex);

        return strlen((char*)buffer);
    }

    if(ismsg){
        // shift the sequence numbers
        int last = SM[socket].rwnd.sequence_numbers[0];
        for(int k=0; k<MAX_WINDOW_SIZE; k++){
            SM[socket].rwnd.sequence_numbers[k] = SM[socket].rwnd.sequence_numbers[k+1];
        }
        SM[socket].rwnd.sequence_numbers[MAX_WINDOW_SIZE-1] = last;
        SM[socket].rwnd.size++;
    }
    signal_sem(mutex);

    if(!ismsg){
        errno = ENOMSG;
        return -1;
    }

    return -1;
}

int check_empty(int socket){
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);
    init_sem();
    int flag = 0;
    wait_sem(mutex);
    for(int i=0; i<MAX_WINDOW_SIZE*2; i++){
        // printf("send_buffer_empty[%d] = %d, send_buffer[%d] = %d\n", i, SM[socket].send_buffer_empty[i], i, SM[socket].send_buffer[i]);
        if(SM[socket].send_buffer_empty[i] == 0){
            flag = 1;
        }
    }
    
    for(int i= 0; i<MAX_WINDOW_SIZE; i++){
        // printf("recv_buffer_empty[%d] = %d, recv_buffer[%d] = %s\n", i, SM[socket].recv_buffer_empty[i], i, SM[socket].recv_buffer[i]);
        if(SM[socket].recv_buffer_empty[i] == 0){
            flag = 1;
        }
    }
    signal_sem(mutex);
    return flag;
}

int m_close(int socket){

    // get SM shared memory
    key_t key1 = ftok(".", 'b');
    int shmid1 = shmget(key1, N*sizeof(SM_), 0777|IPC_CREAT);
    SM_ *SM = (SM_*)shmat(shmid1, 0, 0);

    while(check_empty(socket)){
        sleep(1);
    }
    printf(" CLOSING....\n");

    // get sockinfo shared memory
    key_t key = ftok(".", 'a');
    int shmid = shmget(key, sizeof(sockinfo), 0777|IPC_CREAT);
    // initialize the shared memory
    sockinfo *SOCK_INFO = (sockinfo*)shmat(shmid, 0, 0);

    init_sem();

    SOCK_INFO->sockfd = SM[socket].udp_sockfd;
    SOCK_INFO->port = -1;
    signal_sem(sem1);

    wait_sem(sem2);
    reset();

    wait_sem(mutex);
    close(SM[socket].udp_sockfd);
    SM[socket].free = 1;
    SM[socket].pid = 0;
    SM[socket].udp_sockfd = 0;
    SM[socket].port_other = 0;
    memset(SM[socket].ip_other, 0, sizeof(SM[socket].ip_other));
    memset(SM[socket].send_buffer, 0, sizeof(SM[socket].send_buffer));
    memset(SM[socket].recv_buffer, 0, sizeof(SM[socket].recv_buffer));
    for(int j = 0; j<MAX_WINDOW_SIZE*2; j++){
        SM[socket].send_buffer_empty[j] = 1;
        SM[socket].sent_unack[j] = 0;
    }
    for(int j = 0; j<MAX_WINDOW_SIZE; j++){
        SM[socket].recv_buffer_empty[j] = 1;
    }
    SM[socket].swnd.size = 5;
    SM[socket].rwnd.size = 5;


    for(int j=0; j<MAX_WINDOW_SIZE*2; j++){
        SM[socket].swnd.sequence_numbers[j] = j;         // max window size = 10 and buffer size also 10? change it in future
    }
    for(int j=0; j<MAX_WINDOW_SIZE; j++){
        SM[socket].rwnd.sequence_numbers[j] = j;
    }
    signal_sem(mutex);
    // memset(SM[socket].sent_unack, 0, sizeof(SM[socket].sent_unack));
    return 0;
}

int dropMessage(float prob){
    // return 0;
    srand(time(0)); 
    float r = (float)rand()/(float)(RAND_MAX);
    FILE *fptr = fopen("dropped.txt", "a");
    fprintf(fptr, "on r = %f\n", r);
    fclose(fptr);
    if(r < prob){
        return 1;
    }
    return 0;
}


