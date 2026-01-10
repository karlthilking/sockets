#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define PORTNO  8080
#define BACKLOG 10
#define STDIN   0
#define STDOUT  1
#define STDERR  2


typedef struct __client_t
{
    char* name;
    int fd;
    struct __client_t* next;
} client_t;

typedef struct __client_list_t
{
    client_t* head;
    int client_count;
} client_list_t;

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
client_list_t* c_list;

void* add_client(client_t* client)
{
    pthread_mutex_lock(&client_mutex);
    
    if (c_list->head == NULL)
    {
        c_list->head = client;
        c_list->client_count++;
    }
    else if (c_list->client_count < BACKLOG)
    {
        client->next = c_list->head;
        c_list->head = client;
        c_list->client_count++;
    }
    printf("added client %s\n", client->name);
    pthread_mutex_unlock(&client_mutex);
    return NULL;
}

void* remove_client(int clientfd)
{
    pthread_mutex_lock(&client_mutex);
    
    client_t* prev = NULL;
    client_t* temp = c_list->head;
    while (temp)
    {
        if (temp->fd == clientfd)
        {
            if (prev == NULL)
                c_list->head = temp->next;
            else
                prev->next = temp->next;
            c_list->client_count--;
            printf("removed client %s\n", temp->name);
        }
        prev = temp;
        temp = temp->next;
    }
    
    pthread_mutex_unlock(&client_mutex);
    return NULL;
}

void* broadcast(char* msg, int sender_fd)
{
    pthread_mutex_lock(&client_mutex);
    
    client_t* temp = c_list->head;
    while (temp)
    {
        if (temp->fd != sender_fd)
        {
            send(temp->fd, msg, strlen(msg), 0);
        }
        temp = temp->next;
    }
    pthread_mutex_unlock(&client_mutex);
    return NULL;
}

void* handle_client(client_t* client)
{
    char buf[512], buf2[512];
    char* msg_header = strdup(client->name);
    strcat(msg_header, ": ");
    int bytes_read;
    
    while (1)
    {
        memset(buf, 0, 512);
        if ((bytes_read = recv(client->fd, buf, 
                               511, 0)) <= 0)
        {
            break;
        }
        strcpy(buf2, msg_header);
        strcat(buf2, buf);
        broadcast(buf2, client->fd);
    }

    remove_client(client->fd);
    close(client->fd);
    free(client->name);
    free(client);
    return NULL;
}

int main(int argc, char* argv[])
{
    int servfd, clientfd;
    int opt = 1;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    c_list = malloc(sizeof(client_list_t));
    c_list->head = NULL;
    c_list->client_count = 0;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORTNO);
    addr.sin_addr.s_addr = INADDR_ANY;

    if ((servfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    if (setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }

    if (bind(servfd, (struct sockaddr*)&addr, addrlen) == -1)
    {
        perror("bind");
        exit(1);
    }

    if (listen(servfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }
    
    char buf[512];
    while (1)
    {
        memset(buf, 0, 512);
        clientfd = accept(servfd,(struct sockaddr*)&addr,
                          &addrlen);
        if (clientfd == -1)
        {
            perror("accept");
            continue;
        }

        send(clientfd, "enter your name: ", 18, 0);
        recv(clientfd, buf, 511, 0);
        buf[strlen(buf) - 2] = '\0';

        client_t* client = malloc(sizeof(client_t));
        client->name = strdup(buf);
        client->fd = clientfd;
        add_client(client);

        pthread_t th;
        pthread_create(&th, NULL, (void*)handle_client,
                       client);
        pthread_detach(th);
    }
    return 0;
}

