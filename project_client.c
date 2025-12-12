/*
 * Simple multi-user chat client to communicate with project_server_v2.c.
 * Connects to the server, spawns a listener thread for inbound messages,
 * and forwards everything typed by the user to the server until /quit or EOF.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <pthread.h>

#define SERV_TCP_PORT 23
#define MAX_SIZE 80

struct client_ctx {
    int sockfd;
    volatile int running;
};

static void fatal(const char *msg) {
    perror(msg);
    exit(1);
}

static void *listener_thread(void *arg) {
    struct client_ctx *ctx = (struct client_ctx *)arg;
    char buf[256];

    while (ctx->running) {
        ssize_t n = read(ctx->sockfd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            // Server closed connection or error
            ctx->running = 0;
            printf("\nMain server disconnected\n");
            fflush(stdout);
            break;
        }

        buf[n] = '\0';
        fputs(buf, stdout);
        fflush(stdout);
    }

    return NULL;
}


int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in serv_addr;
    char sendbuf[MAX_SIZE + 2];
    const char *server_ip = "127.0.0.1";
    int port = SERV_TCP_PORT;

    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fatal("socket");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address: %s\n", server_ip);
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fatal("connect");
    }

    struct client_ctx ctx;
    ctx.sockfd = sockfd;
    ctx.running = 1;

    pthread_t tid;
    if (pthread_create(&tid, NULL, listener_thread, &ctx) != 0) {
        fatal("pthread_create");
    }

    const int stdin_fd = fileno(stdin);
    while (ctx.running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(stdin_fd, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // Poll stdin frequently so we can exit quickly after disconnect.

        int ready = select(stdin_fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            ctx.running = 0;
            break;
        }

        if (!ctx.running) {
            break;
        }

        if (ready == 0 || !FD_ISSET(stdin_fd, &readfds)) {
            continue;
        }

        if (fgets(sendbuf, sizeof(sendbuf), stdin) == NULL) {
            ctx.running = 0;
            break;
        }

        size_t len = strlen(sendbuf);
        if (len == 0) {
            continue;
        }

        if (strncmp(sendbuf, "/quit", 5) == 0) {
            ctx.running = 0;
            break;
        }

        if (sendbuf[len - 1] != '\n') {
            sendbuf[len] = '\n';
            sendbuf[len + 1] = '\0';
            len++;
        }

        ssize_t written = write(sockfd, sendbuf, len);
        if (written <= 0) {
            fprintf(stderr, "\n[Failed to send – connection lost]\n");
            ctx.running = 0;
            break;
        }
    }

    ctx.running = 0;
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    pthread_join(tid, NULL);

    return 0;
}
