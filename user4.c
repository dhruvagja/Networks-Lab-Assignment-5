#include <stdio.h>
#include "msocket.h"
#include <string.h>
#include <stdlib.h>


int main(){
    int sockfd;
    char buffer[MAXLINE];
    char *hello = "Hello from client";

    char IP[16] = "127.0.0.1";
    int PORT = 8081;
    char dest_IP[16] = "127.0.0.1";
    int dest_PORT = 8080;

    sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    printf("socket = %d\n", sockfd);

    int bind_status = m_bind(sockfd, IP, PORT, dest_IP, dest_PORT);

    struct sockaddr_in dest_addr;
    socklen_t dest_len = sizeof(dest_addr);
    int len = -1;

    FILE *fp;
    
    memset(buffer, 0, MAXLINE);
    while(len < 0){
        len = m_recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *) &dest_addr, &dest_len);
        if(len > 0 && strcmp(buffer, "EOF") == 0){
            break;
        }
        if(len > 0) {
            // printf("buff = %s\n", buffer);
            fp = fopen("received.txt", "a");
            fprintf(fp, "%s", buffer);
            fclose(fp);
        }
        memset(buffer, 0, MAXLINE);
        len = -1;
        sleep(1);
    }


    printf("received from user1: %s\n", buffer);

    m_close(sockfd);
}