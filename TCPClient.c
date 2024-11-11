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

void communicate_with_server(int sockfd, const char *message) {
    printf("Awaiting server response...");
    char buffer[MAX];
    write(sockfd, message, strlen(message)); // client sends messages to server
    bzero(buffer, sizeof(buffer)); // empty out the buffer
    read(sockfd, buffer, sizeof(buffer)); // client reads response sent by server
    printf(buffer);
}

int main() 
{
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli; // sockaddr_in designed to store IPv4 address info for sockets (IP, port num, address family)
    char user_input[MAX];

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

    // Take messsages
    while(1) {
        // get input
        fgets(user_input, MAX, stdin); 

        // get rid of the new line character
        user_input[strcspn(user_input, "\n")] = 0; 

        if (strcmp(user_input, "Exit") == 0) {
            communicate_with_server(sockfd, "Exit");
            break;
        } else if (strcmp(user_input, "List") == 0) {
            communicate_with_server(sockfd, "List");
        } else if (strncmp(user_input, "REGISTER#", strlen("REGISTER#")) == 0) {
            communicate_with_server(sockfd, user_input);
        } else if (strchr(user_input, '#') != NULL) {
            communicate_with_server(sockfd, user_input);
        } else {
            communicate_with_server(sockfd, user_input);
        }

    }

    // Close the client socket
    close(sockfd);
}

