#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

// Function to handle GET requests
void handle_get_request(int client_socket, char *request_path, char *serving_directory);

// Function to handle errors
void handle_error(int client_socket, int status_code);

int main(int argc, char *argv[])
{
    // Check if the number of arguments is correct
    if (argc != 3)
    {
        printf("Usage: %s <port> <serving_directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse the port number
    int port = atoi(argv[1]);

    // Open the serving directory
    DIR *serving_dir = opendir(argv[2]);
    if (serving_dir == NULL)
    {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    // Create a TCP socket
    // AF_INET: IPv4 Internet protocols
    // SOCK_STREAM: sequenced two-way data transmisision 
    // 0: protocol to use -> default of socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set the socket options to reuse the address and port
    int optval = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the address and port
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) == -1)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Initialize the set of active sockets
    fd_set active_sockets;
    FD_ZERO(&active_sockets);
    FD_SET(server_socket, &active_sockets);

    // Initialize the array of client sockets
    int client_sockets[MAX_CLIENTS];
    memset(client_sockets, 0, sizeof(client_sockets));

    // Main loop
    while (1)
    {
        // Wait for activity on any of the sockets
        fd_set read_sockets = active_sockets;
        if (select(FD_SETSIZE, &read_sockets, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // Check if there is activity on the server socket
        if (FD_ISSET(server_socket, &read_sockets))
        {
            // Accept the incoming connection
            struct sockaddr_in client_address;
            socklen_t client_address_size = sizeof(client_address);
            int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
            if (client_socket == -1)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // Add the client socket to the set of active sockets
            int i;
            for (i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_sockets[i] == 0)
                {
                    client_sockets[i] = client_socket;
                    FD_SET(client_socket, &active_sockets);
                    break;
                }
            }

            // Check if the maximum number of clients is reached
            if (i == MAX_CLIENTS)
            {
                fprintf(stderr, "Too many clients\n");
                exit(EXIT_FAILURE);
            }
        }

        // Check if there is activity on any of the client sockets
        int i;
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            int client_socket = client_sockets[i];
            if (FD_ISSET(client_socket, &read_sockets))
            {
                // Receive the request from the client
                char buffer[BUFFER_SIZE];
                memset(buffer, 0, sizeof(buffer));
                int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
                if (bytes_received == -1)
                {
                    perror("recv");
                    exit(EXIT_FAILURE);
                }

                // Check if the client closed the connection
                if (bytes_received == 0)
                {
                    close(client_socket);
                    FD_CLR(client_socket, &active_sockets);
                    client_sockets[i] = 0;
                }
                else
                {
                    // Parse the request
                    char method[BUFFER_SIZE];
                    char request_path[BUFFER_SIZE];
                    char http_version[BUFFER_SIZE];
                    sscanf(buffer, "%s %s %s", method, request_path, http_version);

                    // Check if the request method is GET
                    if (strcmp(method, "GET") == 0)
                    {
                        // Handle the GET request
                        handle_get_request(client_socket, request_path, argv[2]);
                    }
                    else
                    {
                        // Handle the error
                        handle_error(client_socket, 400);
                    }
                }
            }
        }
    }

    // Close the server socket
    close(server_socket);

    return 0;
}

void handle_get_request(int client_socket, char *request_path, char *serving_directory)
{
    // Check if the request path is "/"
    if (strcmp(request_path, "/") == 0)
    {
        request_path = "/index.html";
    }

    // Construct the full path of the requested file
    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s%s", serving_directory, request_path);

    // Open the requested file
    int file_descriptor = open(full_path, O_RDONLY);
    if (file_descriptor == -1)
    {
        // Handle the error
        if (errno == ENOENT)
        {
            handle_error(client_socket, 404);
        }
        else
        {
            handle_error(client_socket, 400);
        }
    }
    else
    {
        // Read the file contents into a buffer
        char file_buffer[BUFFER_SIZE];
        memset(file_buffer, 0, sizeof(file_buffer));
        int bytes_read = read(file_descriptor, file_buffer, sizeof(file_buffer));
        if (bytes_read == -1)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        // Send the response headers
        char response_headers[BUFFER_SIZE];
        snprintf(response_headers, sizeof(response_headers), "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n", bytes_read);
        send(client_socket, response_headers, strlen(response_headers), 0);

        // Send the file contents
        send(client_socket, file_buffer, bytes_read, 0);

        // Close the file descriptor
        close(file_descriptor);
    }
}

void handle_error(int client_socket, int status_code)
{
    // Send the response headers
    char response_headers[BUFFER_SIZE];
    snprintf(response_headers, sizeof(response_headers), "HTTP/1.1 %d %s\r\nContent-Length: 0\r\n\r\n", status_code, strerror(status_code));
    send(client_socket, response_headers, strlen(response_headers), 0);
}
