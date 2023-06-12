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

    // Read the request from the client
    n = read(client_fd, buf, BUF_SIZE);
    if (n <= 0)
    {
        perror("read");
        return;
    }

    // Parse the request
    sscanf(buf, "%s %s %s", method, path, protocol);

    // Check if the request method is GET
    if (strcasecmp(method, "GET") != 0)
    {
        // Respond with 400 Bad Request
        sprintf(buf, "%s 400 Bad Request\r\n\r\n", protocol);
        write(client_fd, buf, strlen(buf));
        return;
    }

    // Construct the file path
    sprintf(file_path, "%s%s", dir_path, path);

    // Check if the file exists
    if (stat(file_path, &file_stat) < 0)
    {
        // Respond with 404 Not Found
        sprintf(buf, "%s 404 Not Found\r\n\r\n", protocol);
        write(client_fd, buf, strlen(buf));
        return;
    }

    // Check if the file is a directory
    if (S_ISDIR(file_stat.st_mode))
    {
        // Append index.html to the file path
        sprintf(file_path, "%s%sindex.html", dir_path, path);
        if (stat(file_path, &file_stat) < 0)
        {
            // Respond with 404 Not Found
            sprintf(buf, "%s 404 Not Found\r\n\r\n", protocol);
            write(client_fd, buf, strlen(buf));
            return;
        }
    }

    // Open the file
    file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0)
    {
        perror("open");
        return;
    }

    // Determine the content type
    if (strstr(file_path, ".html"))
    {
        content_type = "text/html";
    }
    else if (strstr(file_path, ".css"))
    {
        content_type = "text/css";
    }
    else if (strstr(file_path, ".js"))
    {
        content_type = "application/javascript";
    }
    else if (strstr(file_path, ".png"))
    {
        content_type = "image/png";
    }
    else if (strstr(file_path, ".jpg") || strstr(file_path, ".jpeg"))
    {
        content_type = "image/jpeg";
    }
    else
    {
        content_type = "application/octet-stream";
    }

    // Respond with 200 OK and the file content
    sprintf(buf, "%s 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", protocol, content_type, file_stat.st_size);
    write(client_fd, buf, strlen(buf));
    while ((len = read(file_fd, buf, BUF_SIZE)) > 0)
    {
        write(client_fd, buf, len);
    }

    // Close the file and the client socket
    close(file_fd);
    close(client_fd);
}

int main(int argc, char *argv[])
{
    int server_fd, client_fd, epoll_fd, n, i;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    struct epoll_event event, events[MAX_EVENTS];

    // Check the number of command-line arguments
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <port> <dir_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create the server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Bind the server socket to the specified address and port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));
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

    // Create the epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
    {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Add the server socket to the epoll instance
    event.data.fd = server_fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0)
    {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    // Event loop
    while (1)
    {
        // Wait for events
        n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0)
        {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        // Handle events
        for (i = 0; i < n; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                // Accept incoming connections
                client_len = sizeof(client_addr);
                client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd < 0)
                {
                    perror("accept");
                    continue;
                }

                // Add the client socket to the epoll instance
                event.data.fd = client_fd;
                event.events = EPOLLIN;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0)
                {
                    perror("epoll_ctl");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                // Handle client requests
                handle_request(events[i].data.fd, argv[2]);
            }
        }
    }

    return 0;
}
