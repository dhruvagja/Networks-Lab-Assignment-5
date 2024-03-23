#include <stdio.h>
#include "msocket.h"
#include <string.h>
#include <stdlib.h>


int main(){
    int sockfd;
    char buffer[MAXLINE];
    // char *hello = "Hello from user3";
    // printf("Message : %s\n", hello);
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

    // int len = m_sendto(sockfd, hello, strlen(hello), 0, (const struct sockaddr *) &dest_addr, dest_PORT);

    // Read from file 7.7-KB.txt

    FILE *fp;
    fp = fopen("text_file.txt", "r");
    if (fp == NULL){
        printf("File not found\n");
        return 0;
    }

    // Copy to buffer
    // Read including end of file
    size_t bytesRead;
    memset(buffer, 0, MAXLINE);
    int count = 0;
    while ((bytesRead = fread(buffer, sizeof(char), 1000, fp)) > 0) {
        // buffer[bytesRead] = '\0'; // Null-terminate the buffer
        count++;
        int len = m_sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
        while(len == -2){
            len = m_sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
            sleep(1);
        }
        memset(buffer, 0, MAXLINE);
    }

    strcpy(buffer, "EOF");
    count++;
    int len = m_sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
    while(len == -2){
        len = m_sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *) &dest_addr, sizeof(dest_addr));
        sleep(1);
    }

    printf("Total actual packets sent: %d\n", count);


    
    m_close(sockfd);
}