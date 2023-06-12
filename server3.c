// |Great! How can I assist you with your C coding needs?
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>
#include <sys/epoll.h>

#define MAX_EVENTS 64
#define BUF_SIZE 1024

// Function to handle client requests
void handle_request(int client_fd, char *dir_path)
{
    char buf[BUF_SIZE];
    char method[BUF_SIZE];
    char path[BUF_SIZE];
    char protocol[BUF_SIZE];
    char file_path[BUF_SIZE];
    char *content_type;
    struct stat file_stat;
    int file_fd, n, len;

    // Read request from client
    n = read(client_fd, buf, BUF_SIZE);
    if (n <= 0)
    {
        perror("read");
        return;
    }

    // Parse request
    sscanf(buf, "%s %s %s", method, path, protocol);

    // Only support GET method
    if (strcmp(method, "GET") != 0)
    {
        dprintf(client_fd, "HTTP/1.1 400 Bad Request\r\n\r\n");
        return;
    }

    // Construct file path
    sprintf(file_path, "%s%s", dir_path, path);
    if (strcmp(path, "/") == 0)
    {
        sprintf(file_path, "%s%s", dir_path, "/index.html");
    }

    // Check if file exists
    if (stat(file_path, &file_stat) < 0)
    {
        dprintf(client_fd, "HTTP/1.1 404 Not Found\r\n\r\n");
        return;
    }

    // Open file
    file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0)
    {
        perror("open");
        return;
    }

    // Determine content type
    if (strstr(file_path, ".html"))
    {
        content_type = "text/html";
    }
    else if (strstr(file_path, ".jpg"))
    {
        content_type = "image/jpeg";
    }
    else if (strstr(file_path, ".png"))
    {
        content_type = "image/png";
    }
    else
    {
        content_type = "text/plain";
    }

    // Send response header
    dprintf(client_fd, "HTTP/1.1 200 OK\r\n");
    dprintf(client_fd, "Content-Type: %s\r\n", content_type);
    dprintf(client_fd, "Content-Length: %ld\r\n", file_stat.st_size);
    dprintf(client_fd, "Connection: keep-alive\r\n");
    dprintf(client_fd, "\r\n");

    // Send file content
    while ((len = read(file_fd, buf, BUF_SIZE)) > 0)
    {
        write(client_fd, buf, len);  
    }

    // Close file
    close(file_fd);
}

int main(int argc, char *argv[])
{
    int server_fd, client_fd, epoll_fd, n, i;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    struct epoll_event event, events[MAX_EVENTS];

    // Check command-line arguments
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <port> <dir_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Bind server socket to port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));
    int optval = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, SOMAXCONN) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
    {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Add server socket to epoll instance
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0)
    {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    // Event loop
    while (1)
    {
        n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0)
        {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < n; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                // New client connection
                client_len = sizeof(client_addr);
                client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd < 0)
                {
                    perror("accept");
                    continue;
                }

                // Add client socket to epoll instance
                event.events = EPOLLIN;
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0)
                {
                    perror("epoll_ctl");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                // Client request
                handle_request(events[i].data.fd, argv[2]);

                // Remove client socket from epoll instance
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) < 0)
                {
                    perror("epoll_ctl");
                    exit(EXIT_FAILURE);
                }

                // Close client socket
                close(events[i].data.fd);
            }
        }
    }

    return 0;
}