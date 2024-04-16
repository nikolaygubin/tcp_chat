#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <common.hpp>

#include <nlohmann/json.hpp>

#define handle_error(msg) \
do {                      \
   perror(msg);           \
   exit(EXIT_FAILURE);    \
} while (0)

#define MSG_BUF_SIZE 256
#define MSG_FROM_PROCESS_SIZE 512
#define NUM_ITER 10
#define NUM_THREADS 10

int flag_to_exit = 0;

pthread_t thread_ids[NUM_THREADS];

pthread_mutex_t thread_mutex;

void signal_SIGINT()
{
    flag_to_exit = 1;
}

void free_thread(pthread_t thread_id)
{
    for (int i = 0; i < NUM_THREADS; i++)
    {
        if (thread_id == thread_ids[i])
        {
            thread_ids[i] = 0;
            break;
        }
    }
}

int find_free_thread(pthread_t *thread_ids)
{
    int i;
    for (i = 0; i < NUM_THREADS; i++)
    {   
        if (thread_ids[i] == 0)
        {
            break;
        }
    }

    return i;
}

void* process_listen_client(void *arg)
{
    int sock_client_fd = *((int *)arg);
    char msg_buf[MSG_BUF_SIZE];


    while (recv(sock_client_fd, msg_buf, MSG_BUF_SIZE, 0) != -1)
    {

    }

    close(sock_client_fd);
    free_thread(pthread_self());

    return NULL;
}

int main()
{
    int sock_server_fd, sock_client_fd;
    struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;
    socklen_t serv_len = sizeof(serv_addr), client_len;

    pthread_attr_t attr;

    int free_thread_idx;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = 0;

    if (pthread_mutex_init(&thread_mutex, NULL) == -1)
    {
        handle_error("pthread_mutex_init(): ");
    }

    memset(thread_ids, 0, sizeof(pthread_t) * NUM_THREADS);

    if (pthread_attr_init(&attr) == -1)
    {
        handle_error("pthread_attr_init(): ");
    }

    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 1)
    {
        handle_error("pthread_attr_setdetachstate(): ");
    }

    if (signal(SIGINT, (__sighandler_t)&signal_SIGINT) != NULL)
    {
        handle_error("signal(): ");
    }

    sock_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_server_fd == -1)
    {
        handle_error("socket(): ");
    }

    if (bind(sock_server_fd, (struct sockaddr *)&serv_addr, serv_len) == -1)
    {
        handle_error("bind(): ");
    }

    if (listen(sock_server_fd, 5) == -1)
    {
        handle_error("listen(): ");
    }

    if (getsockname(sock_server_fd, (struct sockaddr *)&serv_addr, &serv_len) == -1)
    {
        handle_error("getsockname(): ");
    }
    printf("Server port is %d\nServer ip addr is %s\n\n", ntohs(serv_addr.sin_port), inet_ntoa(serv_addr.sin_addr));

    while (!flag_to_exit)
    {
        if ((sock_client_fd = accept(sock_server_fd, (struct sockaddr *)&client_addr, &client_len)) == -1)
        {
            handle_error("connect(): ");
        }

        while (1)
        {
            free_thread_idx = find_free_thread(thread_ids);
            if (free_thread_idx == NUM_THREADS - 1)
            {
                sleep(3);
                continue;
            }
            break;
        }

        pthread_create(&thread_ids[free_thread_idx], &attr, &process_listen_client, &sock_client_fd);
    }

    close(sock_server_fd);

    pthread_mutex_destroy(&thread_mutex);

    exit(EXIT_SUCCESS);
}