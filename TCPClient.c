#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#define MAX 80
#define PORT 8888
#define SA struct sockaddr

int main() 
{
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli; // sockaddr_in designed to store IPv4 address info for sockets (IP, port num, address family)

    // socket file descriptor 
    // 選擇 address family (AF_INET), type of socket (TCP socket) 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Socket creation failed.\n");
        exit(0);
    } else {
        printf("Socket created successfully.\n");
    }
    bzero(&servaddr, sizeof(servaddr));

    // Assign server socket IP address and port
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT); // predefined PORT = 8888
    
    // Connect client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("Failed to connect with the server.\n");
        exit(0);
    } else {
        printf("Successfully connected to the server.\n");
    }

    // 

    // Close the client socket
    close(sockfd);
}

