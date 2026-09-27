#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    if (listen(socket_fd, 1) == -1) {
        perror("listen");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    int client_fd;
    do {
        client_fd = accept(socket_fd, NULL, NULL);
    } while (client_fd == -1 && errno == EINTR);

    if (client_fd == -1) {
        perror("accept");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    puts("Accepted one TCP connection.");

    /* Read through the blank line that ends the request headers. */
    char request[8192] = {0};
    size_t received = 0;
    while (strstr(request, "\r\n\r\n") == NULL) {
        if (received == sizeof(request) - 1) {
            fputs("Request headers are too large.\n", stderr);
            close(client_fd);
            close(socket_fd);
            return EXIT_FAILURE;
        }

        ssize_t count = recv(client_fd, request + received,
                             sizeof(request) - 1 - received, 0);
        if (count == -1 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            if (count == -1) {
                perror("recv");
            } else {
                fputs("Client disconnected before sending headers.\n", stderr);
            }
            close(client_fd);
            close(socket_fd);
            return EXIT_FAILURE;
        }

        received += (size_t)count;
        request[received] = '\0';
    }

    const char default_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: 10\r\n"
        "Connection: close\r\n"
        "\r\n"
        " test http";

    const char health_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: 22\r\n"
        "Connection: close\r\n"
        "\r\n"
        "testing a pull request";

    char path[sizeof(request)] = {0};
    sscanf(request, "%*s %8191s", path);
    path[strcspn(path, "?")] = '\0';
    const char *response = strcmp(path, "/health") == 0
                               ? health_response : default_response;
    size_t response_length = strlen(response);

    size_t sent = 0;
    while (sent < response_length) {
        ssize_t count = send(client_fd, response + sent,
                             response_length - sent, MSG_NOSIGNAL);
        if (count == -1 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            if (count == -1) {
                perror("send");
            } else {
                fputs("send made no progress.\n", stderr);
            }
            close(client_fd);
            close(socket_fd);
            return EXIT_FAILURE;
        }
        sent += (size_t)count;
    }

    if (close(client_fd) == -1) {
        perror("close client");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    if (close(socket_fd) == -1) {
        perror("close");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS; /*testing!!!!*/
}
