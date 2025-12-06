#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h> // Using for threads, to give ability to handle numerous users

#define SERV_TCP_PORT 23 /* default port number */
#define MAX_SIZE 80
#define MAX_CLIENTS 15 // default client amount accepted by server

int client_sockets[MAX_CLIENTS]; // Array to keep track of users
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void server_messager(int clientfd, const char *msg, int len){ //Function for server to send user message to users
pthread_mutex_lock(&client_mutex);

for (int i = 0; i < MAX_CLIENTS; i++){
    int fd = client_sockets[i];
    if (fd != -1 && fd != clientfd) {
        write(fd, msg, len);
    }
}

pthread_mutex_unlock(&client_mutex);
}

/* Thread creator, since would be obnoxious to write same code for every client */
void * thread_creator(void *arg){
int clientfd = *(int *)arg;
free(arg);

char string[MAX_SIZE + 1];
int len;

while(1){
    len = read(clientfd, string, MAX_SIZE);
    if (len <= 0){
        break; // If client closed connection or error
    }
    string[len] = 0;

    server_messager(clientfd, string, len);
}

/* Removal of client from list */
for (int i = 0; i < MAX_CLIENTS; i++){
    if (client_sockets[i] == clientfd){
        client_sockets[i] = -1;
        break;
    }
}
pthread_mutex_unlock(&client_mutex);

close(clientfd);
printf("Client %d disconnected \n", clientfd);
return NULL;
}

int main (int argc, char *argv[]){

int sockfd, newsockfd;
socklen_t clilen;
struct sockaddr_in cli_addr, serv_addr;
int port;
char string[MAX_SIZE];
int len;

/* command line: server */

if (argc == 2){
    sscanf(argv[1], "%d", &port); // Reading port number if provided
}
else {
    port = SERV_TCP_PORT;
}

/* open TCP socket (Internet Stream Socket) */

if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
perror("Cannot open stream socket");
exit(1);
}
/* bind local address, so client could send to server */

bzero((char *) &serv_addr, sizeof(serv_addr));
serv_addr.sin_family = AF_INET;
serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
serv_addr.sin_port = htons(port);

if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
    perror ("Cannot bind the address");
    exit(1);
}
/* listen to socket */

listen (sockfd, 5);

//To initialize the list
for (int i = 0; i < MAX_CLIENTS; i++){
    client_sockets[i] = -1;
}
for (;;){
    /* wait for connection from client, this is iterative server*/
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    if (newsockfd < 0) {
    perror("Cannot accept connection");
    continue;
    }

    printf ("fd=%d has joined\n", newsockfd);

    // add client to list
    pthread_mutex_lock(&client_mutex);
    int stored = 0;
    for (int i = 0; i < MAX_CLIENTS; i++){
        if (client_sockets[i] == -1){
            client_sockets[i] = newsockfd;
            stored = 1;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
    if(!stored){
        fprintf(stderr, "Too many users, closing extra conenction.\n");
        close(newsockfd);
        continue;
    }

    //creating thread for client
    pthread_t tid;
    int *pclientfd = malloc(sizeof(int));
    if(!pclientfd){
        perror("malloc failed");
        //removing from list and closing
        pthread_mutex_lock(&client_mutex);
        for(int i = 0; i < MAX_CLIENTS; i++){
            if (client_sockets[i] == newsockfd){
                client_sockets[i] = -1;
                break;
            }
        }
        pthread_mutex_unlock(&client_mutex);
        close(newsockfd);
        continue;
    }
    *pclientfd = newsockfd;
    if(pthread_create(&tid, NULL, thread_creator, pclientfd) != 0){
        perror("Thread failed to create");

        pthread_mutex_lock(&client_mutex);
        for(int i = 0; i < MAX_CLIENTS; i++){
            if (client_sockets[i] == newsockfd){
                client_sockets[i] = -1;
                break;
            }
        }
        pthread_mutex_unlock(&client_mutex);
        close(newsockfd);
        free(pclientfd);
        continue;
    }
    pthread_detach(tid); //used since threads not needed to join main thread
}

close(sockfd);
return 0;
}
