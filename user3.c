#include <stdio.h>
#include "msocket.h"
#include <string.h>
#include <stdlib.h>


int main(){
    int sockfd;
    char buffer[MAXLINE];
    char *hello = "Hello from user3";
    printf("Message : %s\n", hello);
    char IP[16] = "127.0.0.1";
    int PORT = 8080;
    char dest_IP[16] = "127.0.0.1";
    int dest_PORT = 8081;

    sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    printf("Socket : %d\n", sockfd);
    int bind_status = m_bind(sockfd, IP, PORT, dest_IP, dest_PORT);
    // printf("Bind status : %d\n", bind_status);
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(dest_IP);

    int len = m_sendto(sockfd, hello, strlen(hello), 0, (const struct sockaddr *) &dest_addr, dest_PORT);

    printf("len = %d \n", len);
    m_close(sockfd);
}