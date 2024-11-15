#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <sys/types.h> 
#include <netinet/in.h>
#include <unistd.h> // read(), write(), close()
#include <pthread.h> // threads
#define MAX 100
#define MAX_CLIENTS 100
#define SA struct sockaddr

typedef struct {
    char username[MAX];
    char ip_address[INET_ADDRSTRLEN];
    int port;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS]; // Array to hold client information
int client_count = 0; // Number of clients currently online
int sockfd;
const char *server_ip;

void parse_register_message(const char *message, char *account_name, int *user_port) {
    // Buffer for parsing account name and port number
    char temp_message[MAX];
    strncpy(temp_message, message, MAX);
    
    // Parse the message in the format "<AccountName>#<PortNumber>"
    char *port_str = strchr(temp_message, '#');
    if (port_str != NULL) {
        *port_str = '\0';               // Terminate the account name
        port_str++;                      // Move to start of port number
        *user_port = atoi(port_str);     // Convert port string to integer
        strncpy(account_name, temp_message, MAX); // Copy account name
    }
}

void communicate_with_another_client(int sockfd, const char *message) {
    char my_user[MAX], pay_amount[MAX], payee_user[MAX];
    
    // Parse the message to extract MyUserAccountName, payAmount, and PayeeUserAccountName
    sscanf(message, "%[^#]#%[^#]#%s", my_user, pay_amount, payee_user);

    // Look up the payee's information in the stored clients array
    int payee_port = -1;
    char payee_ip[INET_ADDRSTRLEN];

    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].username, payee_user) == 0) {
            strcpy(payee_ip, clients[i].ip_address);
            payee_port = clients[i].port;
            break;
        }
    }

    if (payee_port == -1) {
        printf("User %s not found online.\n", payee_user);
        return;
    }

    // Create a socket to connect to the payee client
    int peer_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (peer_sock == -1) {
        printf("Socket creation failed for P2P transaction.\n");
        return;
    }

    struct sockaddr_in peer_addr;
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(payee_port);
    inet_pton(AF_INET, payee_ip, &peer_addr.sin_addr);

    // Connect to the payee client
    if (connect(peer_sock, (SA*)&peer_addr, sizeof(peer_addr)) != 0) {
        printf("Connection to peer %s at %s:%d failed.\n", payee_user, payee_ip, payee_port);
        close(peer_sock);
        return;
    }
    
    // Format the transfer message and send it to the payee
    char transfer_message[MAX];
    snprintf(transfer_message, sizeof(transfer_message), "%s#%s#%s", my_user, pay_amount, payee_user);
    write(peer_sock, transfer_message, strlen(transfer_message));

    // Wait for acknowledgment from the payee
    char ack_buffer[MAX], buffer[MAX];
    bzero(ack_buffer, sizeof(ack_buffer));
    if (read(peer_sock, ack_buffer, sizeof(ack_buffer)) > 0) {
        if (strcmp(ack_buffer, "Transaction Received") == 0) {
            printf("Transaction confirmed by %s.\n", payee_user);
            
            // After transaction confirmed, start waiting response from server
            read(sockfd, buffer, sizeof(buffer));
            printf("%s", buffer);
            write(sockfd, "List", strlen("List"));
            read(sockfd, buffer, sizeof(buffer));
            printf("%s", buffer);
            // Notify the server about the completed transaction in the required format
            //snprintf(transfer_message, sizeof(transfer_message), "%s#%s#%s", payee_user, pay_amount, my_user);
            //write(sockfd, transfer_message, strlen(transfer_message));  // Inform the server of the successful transaction
        }
    } else {
        printf("No acknowledgment received from %s.\n", payee_user);
    }

    // Close the P2P connection
    close(peer_sock);
}

void communicate_with_server(int sockfd, const char *message) {
    char buffer[MAX];
    // Clear the buffer for receiving server responses
    bzero(buffer, sizeof(buffer));
    
    //ssize_t n = read(sockfd, buffer, sizeof(buffer));
    //buffer[n] = '\0';  // Null-terminate the buffer

    if (strcmp(message, "List") == 0) {
        write(sockfd, message, strlen(message)); // Client sends messages to server
        bzero(buffer, sizeof(buffer)); // Empty out the buffer
        // Read response from server
        read(sockfd, buffer, sizeof(buffer));

        // Clear previous client list
        client_count = 0;

        // Parse the server response for List
        char *line = strtok(buffer, "\n");
        int line_num = 0;

        while (line != NULL && client_count < MAX_CLIENTS) {
            line_num++;

            if (line_num == 1) {
                // Account balance
                printf("Account Balance: %s\n", line);
            } else if (line_num == 2) {
                // Server's public key
                printf("Server Public Key: %s\n", line);
            } else if (line_num == 3) {
                // Number of online accounts
                printf("Number of Accounts Online: %s\n", line);
            } else {
                // Parse client info lines for P2P
                ClientInfo client;
                sscanf(line, "%[^#]#%[^#]#%d", client.username, client.ip_address, &client.port);
                clients[client_count++] = client;
                printf("Client %d - Username: %s, IP: %s, Port: %d\n", client_count, client.username, client.ip_address, client.port);
            }

            line = strtok(NULL, "\n");
        }
    } else if (strchr(message, '#') != NULL && strchr(message, '#') != strrchr(message, '#')) {
        // p2p 傳輸
        //write(sockfd, "List", strlen("List")); // Client sends messages to server
        //bzero(buffer, sizeof(buffer)); // Empty out the buffer
        // Read response from server
        communicate_with_another_client(sockfd, message);
        //read(sockfd, buffer, sizeof(buffer));
        //printf("%s", buffer);
    } else {
        // Display single response for non-List commands
        write(sockfd, message, strlen(message)); // Client sends messages to server
        bzero(buffer, sizeof(buffer)); // Empty out the buffer
        read(sockfd, buffer, sizeof(buffer));
        printf("%s", buffer);
    }
}

void* handle_incoming_transactions(void* arg) {
    int listen_sock = *(int*)arg;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    char buffer[MAX];

    while (1) {
        int connfd = accept(listen_sock, (SA*)&peer_addr, &peer_addr_len);
        if (connfd < 0) {
            printf("Failed to accept P2P connection.\n");
            continue;
        }

        // Read the incoming transaction message
        read(connfd, buffer, sizeof(buffer));
        printf("Received P2P transaction: %s\n", buffer);

        // Send acknowledgment back to the payer
        char ack_message[MAX] = "Transaction Received";
        write(connfd, ack_message, strlen(ack_message));
        write(sockfd, buffer, sizeof(buffer));

        // Close the connection
        close(connfd);
    }
}

int main(int argc, char const *argv[]) 
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get server IP and port from command-line arguments
    server_ip = argv[1];
    int port = atoi(argv[2]);

    int connfd;
    struct sockaddr_in servaddr, cli; // sockaddr_in designed to store IPv4 address info for sockets (IP, port num, address family)
    char user_input[MAX];
    char account_name[MAX];
    int user_port = 0;

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
    servaddr.sin_addr.s_addr = inet_addr(server_ip);
    servaddr.sin_port = htons(port); // predefined PORT = 8888

    printf("IP: %s, port: %d\n", server_ip, port);
    
    // Connect client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("Failed to connect with the server.\n");
        exit(0);
    } else {
        printf("Successfully connected to the server.\n");
    }

    // After connecting to the server, start the listening socket and the thread
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        printf("Listening socket creation failed.\n");
        exit(0);
    }
/*
    struct sockaddr_in self_addr;
    bzero(&self_addr, sizeof(self_addr));
    self_addr.sin_family = AF_INET;
    self_addr.sin_port = htons(user_port);  // Specify the port you want to listen on
    self_addr.sin_addr.s_addr = INADDR_ANY;  // Use any available local address


    if (bind(listen_sock, (SA*)&self_addr, sizeof(self_addr)) != 0) {
        printf("Binding failed on port %d.\n", user_port);
        exit(0);
    }
    if (listen(listen_sock, 5) != 0) {
        printf("Listening failed on port %d.\n", user_port);
        exit(0);
    }
    printf("Listening for P2P connections on port %d\n", user_port);

    // Start the thread for handling incoming transactions
    pthread_t tid;
    pthread_create(&tid, NULL, handle_incoming_transactions, (void*)&listen_sock);

*/
    // Take messsages
    while(1) {
        // get input
        fgets(user_input, MAX, stdin); 

        // get rid of the new line character
        user_input[strcspn(user_input, "\n")] = 0; 

        if (strcmp(user_input, "Exit") == 0) {
            // user 離開
            communicate_with_server(sockfd, "Exit");
            break;
        } else if (strcmp(user_input, "List") == 0) {
            // 列清單
            communicate_with_server(sockfd, "List");
        } else if (strncmp(user_input, "REGISTER#", strlen("REGISTER#")) == 0) {
            // 註冊
            communicate_with_server(sockfd, user_input);
        } else if (strchr(user_input, '#') != NULL && strchr(user_input, '#') != strrchr(user_input, '#')) {
            // 和另一個 client 進行 p2p 轉帳
            communicate_with_server(sockfd, user_input);
        } else if (strchr(user_input, '#') != NULL && strchr(user_input, '#') == strrchr(user_input, '#')) {
            // 登入
            // Login and set up P2P listening socket
            parse_register_message(user_input, account_name, &user_port);
            communicate_with_server(sockfd, user_input);            

            // Create a listening socket after login with the user_port
            listen_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (listen_sock < 0) {
                printf("Listening socket creation failed.\n");
                exit(0);
            }

            struct sockaddr_in self_addr;
            bzero(&self_addr, sizeof(self_addr));
            self_addr.sin_family = AF_INET;
            self_addr.sin_port = htons(user_port);
            self_addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(listen_sock, (SA*)&self_addr, sizeof(self_addr)) != 0) {
                printf("Binding failed on port %d.\n", user_port);
                close(listen_sock);
                exit(0);
            }
            if (listen(listen_sock, 5) != 0) {
                printf("Listening failed on port %d.\n", user_port);
                close(listen_sock);
                exit(0);
            }
            printf("Listening for P2P connections on port %d\n", user_port);

            // Start a thread for handling incoming transactions
            pthread_t tid;
            pthread_create(&tid, NULL, handle_incoming_transactions, (void*)&listen_sock);
        }
        else {
            // 錯誤訊息
            communicate_with_server(sockfd, user_input);
        }

        // 清理 user_input
        bzero(user_input, sizeof(user_input));

    }

    // Close the client socket
    close(sockfd);
}