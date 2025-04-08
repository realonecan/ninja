#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 55552
#define BUFFER_SIZE 1024
#define MAX_USERNAME 16
#define MAX_ROOM 6
#define MAX_CLIENTS 100

typedef struct {
    int sockfd;
    char username[MAX_USERNAME];
    char room[MAX_ROOM];
    int is_owner;
    int online;
} Client;

Client* clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast(Client* sender, char* message) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i] != sender && strcmp(clients[i]->room, sender->room) == 0 && clients[i]->online) {
            send(clients[i]->sockfd, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&mutex);
}

void send_room_status(Client* client) {
    char status[BUFFER_SIZE] = "Room status:\n";
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i]->room, client->room) == 0) {
            char line[64];
            snprintf(line, sizeof(line), "%s (%s%s)\n", 
                     clients[i]->username, 
                     clients[i]->online ? "online" : "offline",
                     clients[i]->is_owner ? ", owner" : "");
            strncat(status, line, BUFFER_SIZE - strlen(status) - 1);
        }
    }
    pthread_mutex_unlock(&mutex);
    send(client->sockfd, status, strlen(status), 0);
}

void* handle_client(void* arg) {
    Client* client = (Client*)arg;
    char buffer[BUFFER_SIZE];
    int bytes = recv(client->sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0) {
        close(client->sockfd);
        free(client);
        return NULL;
    }
    buffer[bytes] = '\0';

    // Parse username:room:is_owner
    char is_owner_str[2];
    sscanf(buffer, "%[^:]:%[^:]:%s", client->username, client->room, is_owner_str);
    client->is_owner = (strcmp(is_owner_str, "1") == 0);
    client->online = 1;

    pthread_mutex_lock(&mutex);
    clients[client_count++] = client;
    pthread_mutex_unlock(&mutex);

    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg), "%s joined the room\n", client->username);
    broadcast(client, join_msg);
    send_room_status(client);

    while (1) {
        bytes = recv(client->sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            client->online = 0;
            snprintf(join_msg, sizeof(join_msg), "%s left the room\n", client->username);
            broadcast(client, join_msg);
            close(client->sockfd);
            break;
        }
        buffer[bytes] = '\0';

        if (client->is_owner && strncmp(buffer, "/kick ", 6) == 0) {
            char target[MAX_USERNAME];
            sscanf(buffer + 6, "%s", target);
            for (int i = 0; i < client_count; i++) {
                if (strcmp(clients[i]->username, target) == 0 && strcmp(clients[i]->room, client->room) == 0) {
                    clients[i]->online = 0;
                    close(clients[i]->sockfd);
                    snprintf(join_msg, sizeof(join_msg), "%s was kicked\n", target);
                    broadcast(client, join_msg);
                    break;
                }
            }
        } else if (client->is_owner && strcmp(buffer, "/delete\n") == 0) {
            for (int i = 0; i < client_count; i++) {
                if (strcmp(clients[i]->room, client->room) == 0) {
                    clients[i]->online = 0;
                    close(clients[i]->sockfd);
                }
            }
            snprintf(join_msg, sizeof(join_msg), "Room %s deleted by owner\n", client->room);
            broadcast(client, join_msg);
            close(client->sockfd);
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < client_count;) {
                if (strcmp(clients[i]->room, client->room) == 0) {
                    free(clients[i]);
                    clients[i] = clients[--client_count];
                } else {
                    i++;
                }
            }
            pthread_mutex_unlock(&mutex);
            return NULL;
        } else {
            char msg[BUFFER_SIZE];
            snprintf(msg, sizeof(msg), "%s: %s", client->username, buffer);
            broadcast(client, msg);
        }
    }
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(1);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }
        Client* client = malloc(sizeof(Client));
        client->sockfd = client_fd;
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, client);
        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}