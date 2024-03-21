#include <stdio.h>
#include "msocket.h"
#include <string.h>
#include <stdlib.h>


int main(){
    int sockfd;
    char buffer[MAXLINE];
    char *hello = "Hello from client";
    char *msg2 = "This is second mesg";
    char *msg3 = "This is third mesg";
    char *msg4 = "This is fourth mesg";
    char *msg5 = "This is fifth mesg";
    char *msg6 = "This is sixth mesg";
    char *msg7 = "This is seventh mesg";
    char *msg8 = "This is eighth mesg";
    char *msg9 = "This is ninth mesg";
    char *msg10 = "This is tenth mesg";
    char *msg11 = "This is eleventh mesg";
    printf("Message : %s\n", hello);
    char IP[16] = "127.0.0.1";
    int PORT = 20000;
    char dest_IP[16] = "127.0.0.1";
    int dest_PORT = 20001;

    sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    printf("Socket : %d\n", sockfd);
    int bind_status = m_bind(sockfd, IP, PORT, dest_IP, dest_PORT);
    // printf("Bind status : %d\n", bind_status);
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(dest_IP);
    sleep(2);
    int len = m_sendto(sockfd, hello, strlen(hello), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    int len2 = m_sendto(sockfd, msg2, strlen(msg2), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    int len3 = m_sendto(sockfd, msg3, strlen(msg3), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    int len4 = m_sendto(sockfd, msg4, strlen(msg4), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    int len5 = m_sendto(sockfd, msg5, strlen(msg5), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    int len6 = m_sendto(sockfd, msg6, strlen(msg6), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    int len7 = m_sendto(sockfd, msg7, strlen(msg7), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    int len8 = m_sendto(sockfd, msg8, strlen(msg8), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    int len9 = m_sendto(sockfd, msg9, strlen(msg9), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    int len10 = m_sendto(sockfd, msg10, strlen(msg10), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    int len11 = m_sendto(sockfd, msg11, strlen(msg11), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));

    printf("len = %d \n", len);
    m_close(sockfd);
}