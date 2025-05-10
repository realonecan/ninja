// server.c – TLS chat server with per-room isolation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <jansson.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "user_auth.h"

#define PORT            55555
#define MAX_CLIENTS     100
#define MAX_ROOM_LEN    8
#define MAX_USER_LEN    32
#define MAX_PASS_LEN    32
#define BUFFER_SIZE     1024

typedef struct {
    char username[MAX_USER_LEN];
    char room    [MAX_ROOM_LEN];
    SSL  *ssl;
    int   sock;
    int   approved;
} Client;

/* ──── global state ──── */
static Client *clients[MAX_CLIENTS];
static int     cli_count = 0;
static pthread_mutex_t cli_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t running = 1;

/* ──── utils ──── */
static void add_client(Client *c)
{
    pthread_mutex_lock(&cli_lock);
    if (cli_count < MAX_CLIENTS) clients[cli_count++] = c;
    pthread_mutex_unlock(&cli_lock);
}

static void remove_client(Client *c)
{
    pthread_mutex_lock(&cli_lock);
    for (int i = 0; i < cli_count; ++i) {
        if (clients[i] == c) {
            for (int j = i + 1; j < cli_count; ++j)
                clients[j - 1] = clients[j];
            --cli_count;
            break;
        }
    }
    pthread_mutex_unlock(&cli_lock);
}

static void broadcast(const char *msg, const char *room, Client *sender)
{
    pthread_mutex_lock(&cli_lock);
    for (int i = 0; i < cli_count; ++i) {
        Client *c = clients[i];
        if (c != sender && strcmp(c->room, room) == 0) {
            SSL_write(c->ssl, msg, strlen(msg));
        }
    }
    pthread_mutex_unlock(&cli_lock);
}

/* ──── TLS helpers ──── */
static SSL_CTX *init_ssl_ctx(void)
{
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());

    if (!SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) ||
        !SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM)) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

/* ──── room DB (unchanged) ──── */
static void save_room_if_not_exists(const char *room,
                                    const char *owner,
                                    const char *owner_pass,
                                    const char *public_pass)
{
    json_error_t e;
    json_t *root = json_load_file("rooms.json", 0, &e);
    if (!root) root = json_object();

    if (!json_object_get(root, room)) {
        json_t *data = json_object();
        json_object_set_new(data, "owner",       json_string(owner));
        json_object_set_new(data, "owner_pass",  json_string(owner_pass));
        json_object_set_new(data, "public_pass", json_string(public_pass));
        json_object_set_new(root, room, data);
        json_dump_file(root, "rooms.json", JSON_INDENT(2));
    }
    json_decref(root);
}

/* ──── client thread ──── */
static void *client_worker(void *arg)
{
    Client *cli = arg;

    /* fake approval */
    if (!cli->approved) {
        SSL_write(cli->ssl, "WAITING_APPROVAL", 16);
        sleep(2);
        cli->approved = 1;
    }
    SSL_write(cli->ssl, "APPROVED", 8);
    add_client(cli);

    char buf[BUFFER_SIZE];
    int  n;
    while ((n = SSL_read(cli->ssl, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        broadcast(buf, cli->room, cli);
    }

    /* cleanup */
    remove_client(cli);
    SSL_shutdown(cli->ssl);
    SSL_free(cli->ssl);
    close(cli->sock);
    free(cli);
    return NULL;
}

/* ──── signal handler ──── */
static void on_sigint(int sig) { (void)sig; running = 0; }

int main(void)
{
    signal(SIGINT, on_sigint);

    init_user_file();
    SSL_CTX *ctx = init_ssl_ctx();

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(srv, (void *)&addr, sizeof(addr));
    listen(srv, 5);

    printf("🔒 Ninja Chat server on :%d – Ctrl-C to stop\n", PORT);

    while (running) {
        struct sockaddr_in caddr; socklen_t len = sizeof(caddr);
        int cfd = accept(srv, (void *)&caddr, &len);
        if (cfd < 0) continue;

        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, cfd);
        if (SSL_accept(ssl) <= 0) { close(cfd); SSL_free(ssl); continue; }

        /* first packet: username:room:public:owner:is_owner */
        char hello[BUFFER_SIZE];
        int  m = SSL_read(ssl, hello, sizeof(hello) - 1);
        if (m <= 0) { SSL_free(ssl); close(cfd); continue; }
        hello[m] = '\0';

        char user[MAX_USER_LEN], room[MAX_ROOM_LEN];
        char pub_pass[MAX_PASS_LEN], own_pass[MAX_PASS_LEN];
        int  is_owner = 0;
        sscanf(hello, "%31[^:]:%7[^:]:%31[^:]:%31[^:]:%d",
               user, room, pub_pass, own_pass, &is_owner);

        if (is_owner)
            save_room_if_not_exists(room, user, own_pass, pub_pass);

        Client *cli = calloc(1, sizeof(*cli));
        strncpy(cli->username, user, sizeof(cli->username) - 1);
        strncpy(cli->room,     room, sizeof(cli->room)  - 1);
        cli->ssl      = ssl;
        cli->sock     = cfd;
        cli->approved = 0;

        pthread_t tid;
        pthread_create(&tid, NULL, client_worker, cli);
        pthread_detach(tid);
    }

    close(srv);
    SSL_CTX_free(ctx);
    printf("\nGraceful shutdown complete.\n");
    return 0;
}
