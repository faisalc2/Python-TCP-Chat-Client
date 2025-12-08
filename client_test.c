/* 
CMPSC 311 
Chat Application

This file is to test server functionality before fully built front-end.


compile cmd:
gcc -o client_test2 client_test.c -pthread
*/



#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#define SERV_TCP_PORT 23 // SERVER PORT

int sockfd;

void *receivemessage(void *arg) {
    char msg[512];
    while (1){
        int bytes = recv(sockfd, msg, sizeof(msg) -1, 0);
        if (bytes <= 0) break;
        msg[bytes] = '\0';
        printf("%s", msg);
    }
    return NULL;

}


int main(int argc, char *argv[])
{
    
    struct sockaddr_in serv_addr;
    char *serv_host = "localhost";
    struct hostent *host_ptr;
    int port = SERV_TCP_PORT;;
    int buff_size = 0;
    pthread_t listenthread;
    char msg[512];

    /* command line: client [host [port]]*/

    if(argc >= 2)
        serv_host = argv[1]; /* read the host if provided */
    if(argc == 3)
        sscanf(argv[2], "%d", &port); /* read the port if provided */

    /* get the address of the host */
    if((host_ptr = gethostbyname(serv_host)) == NULL) 
    {
     perror("gethostbyname error");
     exit(1);
    }

    if(host_ptr->h_addrtype !=  AF_INET)
    {
        perror("unknown address type");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr =
        ((struct in_addr *)host_ptr->h_addr_list[0])->s_addr;
    serv_addr.sin_port = htons(port);

    /* open a TCP socket */
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("can't open stream socket");
        exit(1);
    }

    // connect to the server

    if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
        perror("can't connect to server");
        exit(1);
    }
     
    printf("Connected to server! type /q to quit. \n");
    

    /* listen for messages and print as they come */
    pthread_create(&listenthread, NULL, receivemessage, NULL);

    // loop for writing messages  ( and to quit ! )
    while(1){
        fgets(msg, sizeof(msg), stdin);
        if (strncmp(msg, "/q", 2) == 0) break;
        send(sockfd, msg, strlen(msg), 0);

    }


    // close connection
    close(sockfd);
    printf("Disconnected. \n");
    return 0;


}