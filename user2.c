#include <stdio.h>
#include "msocket.h"
#include <string.h>
#include <stdlib.h>


int main(){
    int sockfd;
    char buffer[MAXLINE];
    char *hello = "Hello from client";

    char IP[16] = "127.0.0.1";
    int PORT = 20001;
    char dest_IP[16] = "127.0.0.1";
    int dest_PORT = 20000;

    sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    printf("socket = %d\n", sockfd);

    int bind_status = m_bind(sockfd, IP, PORT, dest_IP, dest_PORT);

    struct sockaddr_in dest_addr;
    socklen_t dest_len = sizeof(dest_addr);
    int len = -1;

    while(len < 0){
        len = m_recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *) &dest_addr, &dest_len);
        sleep(1);
    }
    printf("received from user1: %s\n", buffer);

    m_close(sockfd);
}