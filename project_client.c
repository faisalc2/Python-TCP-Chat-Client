
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

/*
  Shared context between main thread (sending/input) and listener thread (receiving).
  - sockfd: the connected TCP socket
  - running: a "stop flag" used to signal shutdown when server disconnects or /quit happens
*/

struct client_ctx {
    int sockfd;
    volatile int running;
};

static void fatal(const char *msg) {
    perror(msg);
    exit(1);
}

/*
  Receiver thread:
  Continuously reads from the server socket and prints anything received to stdout.
  This allows the client to receive messages while the main thread is waiting for user input.
*/

static void *listener_thread(void *arg) {
    struct client_ctx *ctx = (struct client_ctx *)arg;
    char buf[256];

    while (ctx->running) {
        ssize_t n = read(ctx->sockfd, buf, sizeof(buf) - 1);
        /*
          Blocking read on the socket:
          - n > 0 : received data
          - n == 0: peer performed an orderly shutdown (server closed)
          - n < 0 : error (could be connection reset, etc.)
        */
        if (n <= 0) {
            // Server closed connection or error
            ctx->running = 0;
            printf("\nMain server disconnected\n");
            fflush(stdout);
            break;
        }
        // Make it a C string so we can print safely
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
    // Defaults: connect to localhost on default port
    const char *server_ip = "127.0.0.1";
    int port = SERV_TCP_PORT;

    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }

    /* Create TCP socket (IPv4, stream) */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fatal("socket");
    }

    /*
      Fill in server address struct:
      - sin_family: IPv4
      - sin_port: network-byte-order port
      - sin_addr: converted from text IP to binary form
    */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address: %s\n", server_ip);
        exit(1);
    }

    /* Connect to the server */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fatal("connect");
    }

    // Initialize shared context for both threads
    struct client_ctx ctx;
    ctx.sockfd = sockfd;
    ctx.running = 1;

    /*
      Start background listener thread:
      It will print server messages as they arrive, independently of user input.
    */
    pthread_t tid;
    if (pthread_create(&tid, NULL, listener_thread, &ctx) != 0) {
        fatal("pthread_create");
    }

    const int stdin_fd = fileno(stdin);
    while (ctx.running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(stdin_fd, &readfds);

        /*
          Timeout for select():
          - Here: 50,000 microseconds = 50ms
          - That means: even if user does nothing, loop wakes up frequently to check ctx.running
        */
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // Poll stdin frequently so we can exit quickly after disconnect.

        /*
          select() waits until stdin is readable OR timeout occurs.
          stdin_fd + 1 is required because select expects "max fd + 1".
        */
        int ready = select(stdin_fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) {
                // Interrupted by a signal; retry
                continue;
            }
            perror("select");
            ctx.running = 0;
            break;
        }

        // If listener thread marked running=0 (server disconnected), exit
        if (!ctx.running) {
            break;
        }

        // Timeout (ready==0) or stdin not set -> loop again
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

        /*
          Local quit command:
          "/quit" triggers graceful shutdown of client.
          (This is client-side only.)
        */

        if (strncmp(sendbuf, "/quit", 5) == 0) {
            ctx.running = 0;
            break;
        }

        /*
          Ensure message ends with newline, because server logic uses read()+newline trimming
          and broadcasts messages as lines.
          If user typed a long line without newline (buffer full), add one.
        */
        if (sendbuf[len - 1] != '\n') {
            sendbuf[len] = '\n';
            sendbuf[len + 1] = '\0';
            len++;
        }

        /*
          Send message to server.
          Note: write() can write fewer bytes than requested in rare cases.
          For small lines on TCP, it often writes all, but a "send_all" loop is safer.
        */
        ssize_t written = write(sockfd, sendbuf, len);
        if (written <= 0) {
            fprintf(stderr, "\n[Failed to send – connection lost]\n");
            ctx.running = 0;
            break;
        }
    }

    /*
      Cleanup sequence:
      - running=0 so listener thread can exit
      - shutdown() interrupts blocking socket reads/writes
      - close socket
      - join listener thread so we don’t exit while it is still running
    */
    ctx.running = 0;
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    pthread_join(tid, NULL);

    return 0;
}
