#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <jansson.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "user_auth.h"

#define PORT 55555
#define MAX_CLIENTS 100
#define MAX_ROOM_LEN 6
#define MAX_USER_LEN 32
#define MAX_PASS_LEN 32
#define BUFFER_SIZE 1024

typedef struct {
    char username[MAX_USER_LEN];
    char room[MAX_ROOM_LEN];
    SSL* ssl;
    int socket;
    int approved;
} Client;

Client* active_clients[MAX_CLIENTS];
int active_count = 0;

// Load or create rooms.json
void save_room_if_not_exists(const char* room, const char* owner, const char* owner_pass, const char* public_pass) {
    json_error_t error;
    json_t* root = json_load_file("rooms.json", 0, &error);
    if (!root) root = json_object();

    if (!json_object_get(root, room)) {
        json_t* room_data = json_object();
        json_object_set_new(room_data, "owner", json_string(owner));
        json_object_set_new(room_data, "owner_pass", json_string(owner_pass));
        json_object_set_new(room_data, "public_pass", json_string(public_pass));
        json_object_set_new(root, room, room_data);
        json_dump_file(root, "rooms.json", JSON_INDENT(2));
    }
    json_decref(root);
}

SSL_CTX* init_server_ssl() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);

    if (!SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) ||
        !SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM)) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void broadcast(const char* msg, Client* sender) {
    for (int i = 0; i < active_count; i++) {
        if (active_clients[i] != sender) {
            SSL_write(active_clients[i]->ssl, msg, strlen(msg));
        }
    }
}

void* handle_client(void* arg) {
    Client* client = (Client*)arg;

    if (!client->approved) {
        const char* wait_msg = "WAITING_APPROVAL";
        SSL_write(client->ssl, wait_msg, strlen(wait_msg));
        sleep(2); // simulate owner approval step
        client->approved = 1;
    }

    SSL_write(client->ssl, "APPROVED", 8);
    active_clients[active_count++] = client;

    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = SSL_read(client->ssl, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[bytes] = '\0';
        broadcast(buffer, client);
    }

    close(client->socket);
    SSL_free(client->ssl);
    free(client);
    return NULL;
}

int main() {
    SSL_CTX* ctx = init_server_ssl();
    init_user_file();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("🔒 Secure Ninja Chat Server running on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &len);

        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_fd);
        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            close(client_fd);
            continue;
        }

        char buffer[BUFFER_SIZE];
        int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) continue;
        buffer[bytes] = '\0';

        // Parse format: username:room:public_pass:owner_pass:is_owner
        char username[MAX_USER_LEN], room[MAX_ROOM_LEN];
        char public_pass[MAX_PASS_LEN], owner_pass[MAX_PASS_LEN];
        int is_owner = 0;

        sscanf(buffer, "%[^:]:%[^:]:%[^:]:%[^:]:%d",
               username, room, public_pass, owner_pass, &is_owner);

        if (is_owner) {
            save_room_if_not_exists(room, username, owner_pass, public_pass);
        }

        Client* client = malloc(sizeof(Client));
        strncpy(client->username, username, MAX_USER_LEN);
        strncpy(client->room, room, MAX_ROOM_LEN);
        client->ssl = ssl;
        client->socket = client_fd;
        client->approved = 0;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client);
        pthread_detach(tid);
    }

    SSL_CTX_free(ctx);
    close(server_fd);
    return 0;
}
