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
#define MAX_CHAR_NAME 32 //default amount for username length
#define MAX_SERVER_NAME 64

int client_sockets[MAX_CLIENTS]; // Array to keep track of users
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

char usernames[MAX_CLIENTS][MAX_CHAR_NAME];
char server_name[MAX_SERVER_NAME];

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

int slot = -1;

//Finding index in client_sockets
pthread_mutex_lock(&client_mutex);
for (int i = 0; i < MAX_CLIENTS; i++){
    if(client_sockets[i] == clientfd){
        slot = i;
        break;
    }

}
pthread_mutex_unlock(&client_mutex);

if(slot == -1){
    printf("Error: couldn't find slot for client fd %d\n", clientfd);
    close(clientfd);
    return NULL;
}

/* Notify the user about the server they're joining */
char welcome_msg[128];
snprintf(welcome_msg, sizeof(welcome_msg), "Welcome to the %s\n", server_name);
write(clientfd, welcome_msg, strlen(welcome_msg));

/*Prompt for user to enter username*/
const char *prompt = "Enter username: ";
write(clientfd, prompt, strlen(prompt));

len = read(clientfd, string, MAX_SIZE);
if (len <= 0){
    goto cleanup; //if client disconnected before sending username
}
string[len] = 0;

//Making sure newlines not messing around
char *p = strchr(string, '\n');
if(p) {*p = 0;}
p = strchr(string, '\r');
if(p) {*p = 0;}

/*Storing usernames*/
pthread_mutex_lock(&client_mutex);
strncpy(usernames[slot], string, MAX_CHAR_NAME - 1);
usernames[slot][MAX_CHAR_NAME - 1] = '\0';
pthread_mutex_unlock(&client_mutex);

printf("Client fd=%d is now '%s' (slot=%d)\n", clientfd, usernames[slot], slot);

//Notification for other users that new one joined
char joinsmg[128];
snprintf(joinsmg, sizeof(joinsmg), "[%s joined the chat] \n", usernames[slot]);
server_messager(clientfd, joinsmg, strlen(joinsmg));


while(1){
    len = read(clientfd, string, MAX_SIZE);
    if (len <= 0){
        break; // If client closed connection or error
    }
    string[len] = 0;

//Making sure newlines not messing around
char *p = strchr(string, '\n');
if(p) {*p = 0;}
p = strchr(string, '\r');
if(p) {*p = 0;}

// Necessary for making message as "Username: message"
char outbuf[MAX_SIZE + MAX_CHAR_NAME + 4];
snprintf(outbuf, sizeof(outbuf), "%s: %s\n", usernames[slot], string);

    server_messager(clientfd, outbuf, strlen(outbuf));
}
cleanup:

    char name[MAX_CHAR_NAME];

/* Removal of client from list */
pthread_mutex_lock(&client_mutex);

strncpy(name, usernames[slot], MAX_CHAR_NAME - 1);
name[MAX_CHAR_NAME -1] = '\0';

for (int i = 0; i < MAX_CLIENTS; i++){
    if (client_sockets[i] == clientfd){
        client_sockets[i] = -1;
        usernames[i][0] = '\0';
        break;
    }
}
pthread_mutex_unlock(&client_mutex);

if(name[0] != '\0') {
    char leftmsg[128];
    snprintf(leftmsg, sizeof(leftmsg), "[%s left the chat] \n", name);
    server_messager(clientfd, leftmsg, strlen(leftmsg));
}

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

printf("Enter server name: ");
fflush(stdout);
if (!fgets(server_name, sizeof(server_name), stdin)) {
    strncpy(server_name, "Server", sizeof(server_name));
    server_name[sizeof(server_name) - 1] = '\0';
} else {
    char *p = strchr(server_name, '\n');
    if (p) {
        *p = '\0';
    }
    if (server_name[0] == '\0') {
        strncpy(server_name, "Server", sizeof(server_name));
        server_name[sizeof(server_name) - 1] = '\0';
    }
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
    usernames[i][0] = '\0';
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
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++){
        if (client_sockets[i] == -1){
            client_sockets[i] = newsockfd;
            usernames[i][0] = '\0';
            slot = i;
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
