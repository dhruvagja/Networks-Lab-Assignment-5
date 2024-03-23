#include <stdio.h>
#include "msocket.h"
#include <string.h>
#include <stdlib.h>


int main(){
    int sockfd;
    char buffer[MAXLINE];
    

    FILE *fp;
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
  

    size_t bytesRead;
    memset(buffer, 0, MAXLINE);
    int count = 0;

    fp = fopen("second_test.txt", "r");
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


    printf("Total packets sent: %d\n", count);

    printf("len = %d \n", len);
    m_close(sockfd);
}