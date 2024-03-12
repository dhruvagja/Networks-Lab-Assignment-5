#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <errno.h>

#include "msocket.h"

void reset(){
    SOCK_INFO.sockfd = 0;
    SOCK_INFO.err = 0;
    SOCK_INFO.port = 0;
    memset(SOCK_INFO.ip, 0, sizeof(SOCK_INFO.ip));
}

void init_sem(){
    semctl(sem1, 0, SETVAL, 0);
    semctl(sem2, 0, SETVAL, 0);

    pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1;

}

int m_socket(int domain, int type, int protocol){
    int free_entry = 0;
    for(int i=0; i<N; i++){
        if(SM[i].free == 1){
            // free_entry = 1;
            // SM[i].free = 0;
            // SM[i].pid = getpid();
            // SM[i].udp_sockfd = socket(domain, type, protocol);

            // return i;

            free_entry = 1;
            signal_sem(&sem1);
            wait_sem(&sem2);

            if(SOCK_INFO.sockfd == -1){
                errno = SOCK_INFO.err;
                return -1;
            }
            else{
                SM[i].free = 0;
                SM[i].pid = getpid();
                SM[i].udp_sockfd = SOCK_INFO.sockfd;
                reset();
                // signal_sem(&sem1);
                return i;
            }
        }
    }

    if(!free_entry){
        //  If no free entry is available, it returns -1 with the global error variable set to ENOBUFS.
        errno = ENOBUFS;
        return -1;
    }
}

int m_bind(int sockid, char *source_ip, int source_port, char *dest_ip, int dest_port){

    if(SM[sockid].udp_sockfd == -1){
        errno = EBADF;
        return -1;
    }

    SOCK_INFO.sockfd = SM[sockid].udp_sockfd;
    SOCK_INFO.port = source_port;
    strcpy(SOCK_INFO.ip, source_ip);


    signal_sem(&sem1);

    wait_sem(&sem2);

    if(SOCK_INFO.sockfd == -1){
        errno = SOCK_INFO.err;
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
    for(int i=0; i<MAX_WINDOW_SIZE*2; i++){
        if(SM[socket].send_buffer_empty[i] == 0){
            isspace = 1;
            SM[socket].send_buffer_empty[i] = 1;
            strcpy(SM[socket].send_buffer[i], message);
            return 0;   // if successful
        }
    }

    // if no space is available in the send buffer, it returns -1 with the global error variable set to ENOBUFS.
    if(!isspace){
        errno = ENOBUFS;
        return -1;
    }
    
}

ssize_t m_recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len){

}

int m_close(int socket){
    close(SM[socket].udp_sockfd);
    SM[socket].free = 1;
    SM[socket].pid = 0;
    SM[socket].udp_sockfd = 0;
    SM[socket].port_other = 0;
    memset(SM[socket].ip_other, 0, sizeof(SM[socket].ip_other));
    memset(SM[socket].send_buffer, 0, sizeof(SM[socket].send_buffer));
    memset(SM[socket].recv_buffer, 0, sizeof(SM[socket].recv_buffer));
    memset(SM[socket].send_buffer_empty, 0, sizeof(SM[socket].send_buffer_empty));  // if empty, 0 else 1
    memset(SM[socket].recv_buffer_empty, 0, sizeof(SM[socket].recv_buffer_empty));
    memset(SM[socket].swnd.sequence_numbers, 0, sizeof(SM[socket].swnd.sequence_numbers));
    memset(SM[socket].rwnd.sequence_numbers, 0, sizeof(SM[socket].rwnd.sequence_numbers));
    return 0;
}
