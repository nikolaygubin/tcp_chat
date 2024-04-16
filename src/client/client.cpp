#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>

#include <common.hpp>

#include <nlohmann/json.hpp>

#define handle_error(msg) \
do {                      \
   perror(msg);           \
   exit(EXIT_FAILURE);    \
} while (0)

#define NUM_ITER 10

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        printf("Usage: ./client ip_addr port delay\n");
        exit(EXIT_FAILURE);
    }

    size_t delay = atoi(argv[3]);

    int sock_fd;
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(atoi(argv[2])),
    };
    socklen_t server_len = sizeof(server_addr);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        handle_error("socket(): ");
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr, server_len) == -1)
    {
        handle_error("connect(): ");
    }

    for (int i = 0; i < NUM_ITER; i++)
    {
        char number = i + '0';
        if (send(sock_fd, &number, 1, 0) == -1)
        {
            handle_error("send(): ");
        }

        sleep(delay);
    }

    close (sock_fd);

    exit(EXIT_SUCCESS);
}