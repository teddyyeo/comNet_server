#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Check if the number of arguments is correct
    if (argc != 4) {
        printf("Usage: %s <server_ip> <server_port> <file_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse the server IP and port number
    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    // Create a TCP socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(server_ip);
    server_address.sin_port = htons(server_port);
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // Send the GET request to the server
    char *file_path = argv[3];
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "GET %s HTTP/1.1\r\n\r\n", file_path);
    if (send(client_socket, request, strlen(request), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    // Receive and print the response from the server
    char response[BUFFER_SIZE];
    memset(response, 0, sizeof(response));
    int bytes_received;
    while ((bytes_received = recv(client_socket, response, sizeof(response) - 1, 0)) > 0) {
        printf("%s", response);
        memset(response, 0, sizeof(response));
    }
    if (bytes_received == -1) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    // Close the client socket
    close(client_socket);

    return 0;
}