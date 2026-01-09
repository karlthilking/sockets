#include <sys/socket.h> // socket, setsockopt, bind, listen, accept, AF_INET,
                        // SOCK_STREAM, SOL_SOCKET, SO_REUSEADDR, socklen_t,
                        // send, recv, struct sockaddr
#include <sys/types.h> // pid_t, 
#include <netinet/in.h> // sockaddr_in, INADDR_ANY, htons
#include <unistd.h> // close, read, write, fork
#include <string.h> // memset, strlen, strcat, strcpy, strdup
#include <stdio.h> // printf
#include <pthread.h> // pthread_t, pthread_mutex_t, pthread_mutex_lock/unlock
                     // PTHREAD_MUTEX_INITIALIZER
#include <stdbool.h> // bool
#include <stdlib.h> // malloc

#define PORTNO 8080
#define BACKLOG 10
#define STDIN 0
#define STDOUT 1

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct __client_t
{
    char name[128];
    int fd;
    struct __client_t* next;
} client_t;

typedef struct __client_list_t
{
    client_t* head;
    int client_count;
} client_list_t;

client_list_t* c_list;

void* add_client(client_t* client)
{
    bool added = false;
    pthread_mutex_lock(&client_mutex);
    
    if (c_list->client_count < BACKLOG)
    {
        added = true;
        client->next = c_list->head;
        c_list->head = client;
        c_list->client_count++;
    }
    
    if (added)
        printf("added client %d\n", client->fd);
    else
        printf("max clients\n");

    pthread_mutex_unlock(&client_mutex);
    return NULL;
}

void* remove_client(int clientfd)
{
    bool removed = false;   
    pthread_mutex_lock(&client_mutex);

    client_t* temp = c_list->head;
    while (temp)
    {
        if ((temp->next)->fd == clientfd)
        {
            removed = true;
            temp->next = (temp->next)->next;
            c_list->client_count--;
        }
    }

    if (removed)
        printf("removed client %d\n", clientfd);
    else
        printf("could not find client %d\n", clientfd);

    pthread_mutex_unlock(&client_mutex);
    return NULL;
}

void* display_clients()
{
    pthread_mutex_lock(&client_mutex);
    
    printf("clients: ");
    client_t* temp = c_list->head;
    while (temp)
    {
        if (temp->next == NULL)
            printf("%s", temp->name);
        else
            printf("%s, ", temp->name);
        temp = temp->next;
    }

    pthread_mutex_unlock(&client_mutex);
    return NULL;
}

bool broadcast_msg(char* msg, int sender_fd)
{
    pthread_mutex_lock(&client_mutex);
    client_t* temp = c_list->head;  
    int bytes_sent;

    while (temp)
    {
        if (temp->fd != sender_fd)
        {
            if ((bytes_sent = send(temp->fd, msg, strlen(msg), 0)) <= 0)
            {
                return false;
            }
        }
        temp = temp->next;
    }
    return true;
}

void* handle_client(client_t* client)
{
    char buf[256];
    char* msg;
    int bytes_read, bytes_sent;

    while (1)
    {
        memset(buf, 0, 256);
        memset(msg, 0, strlen(msg));

        if ((bytes_read = recv(client->fd, buf, 255, 0)) <= 0)
        {
            break;
        }
        
        msg = strdup(client->name);
        msg[strlen(msg)] = '\0';
        strcat(msg, ": ");
        strcat(msg, buf);
        if (!broadcast_msg(msg, client->fd))
        {
            break;
        }
    }
    
    remove_client(client->fd);
    close(client->fd);
    free(client);
    return NULL;
}

int main(int argc, char* argv[])
{
    int servfd, clientfd, opt;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    const char* welcome_msg = "enter your name: ";
    char buf[512];

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORTNO);
    addr.sin_addr.s_addr = INADDR_ANY;
    memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

    c_list = malloc(sizeof(client_list_t));
    c_list->head = NULL;
    c_list->client_count = 0;

    if ((servfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }

    if (setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
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
    
    while (1)
    {
        if (c_list->client_count == 2)
            display_clients();
        memset(buf, 0, 512);
        if ((clientfd = accept(servfd, (struct sockaddr*)&addr, &addrlen)) == -1)
        {
            perror("accept");
            continue;
        }

        send(clientfd, welcome_msg, strlen(welcome_msg), 0);
        recv(clientfd, buf, 511, 0);
        buf[strlen(buf)] = '\0';

        client_t* client = malloc(sizeof(client_t));
        strcpy(client->name, buf);
        client->fd = clientfd;
        add_client(client);
        
        pthread_t th;
        pthread_create(&th, NULL, (void*)handle_client, client);
        pthread_detach(th);
    }

    close(servfd);
    return 0;
}
