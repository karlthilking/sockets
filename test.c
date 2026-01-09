#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORTNO 8080
#define BACKLOG 10
#define STDIN 0
#define STDOUT 1

void* handle_client(int clientfd)
{
    char buf[256];
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        recv(clientfd, buf, sizeof(buf), 0);
        write(STDOUT, buf, strlen(buf));
    }
    return NULL;
}

void* handle_server(int clientfd)
{
    char buf[256];
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        read(STDIN, buf, sizeof(buf));
        send(clientfd, buf, strlen(buf), 0);
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    int sockfd, clientfd, opt;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    pid_t pid;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORTNO);
    addr.sin_addr.s_addr = INADDR_ANY;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }
    else
    {
        printf("socket fd: %d\n", sockfd);
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }
    else
    {
        printf("set socket options\n");
    }

    if (bind(sockfd, (struct sockaddr*)&addr, addrlen) == -1)
    { 
        perror("bind");
        exit(1);
    }
    else
    {
        printf("binded socket\n");
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }
    else
    {
        printf("listening on port %d...\n", PORTNO);
    }
    
    if ((clientfd = accept(sockfd, (struct sockaddr*)&addr,
                           &addrlen)) == -1)
    {
        perror("accept");
        exit(1);
    }
    else
    {
        printf("accepted client (fd: %d)\n", clientfd);
    }
    
    pid = fork();
    switch(pid)
    {
        case -1:
            perror("fork");
            exit(1);
        case 0:
            handle_client(clientfd);
            exit(0);
        default:
            handle_server(clientfd);
            close(sockfd);
            close(clientfd);
            exit(0);
    }
    return 0;
}
