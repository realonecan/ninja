// ninja.c – ncurses TLS client
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "user_auth.h"

#define PORT         55555
#define BUFFER_SIZE  1024

static SSL       *ssl;
static int        sockfd;
static pthread_mutex_t scr_lock = PTHREAD_MUTEX_INITIALIZER;

/* ─── receive thread ─── */
static void *receiver(void *arg)
{
    char buf[BUFFER_SIZE];
    (void)arg;
    while (1) {
        int n = SSL_read(ssl, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';

        pthread_mutex_lock(&scr_lock);
        mvprintw(14, 2, "%s", buf);
        clrtoeol(); refresh();
        pthread_mutex_unlock(&scr_lock);
    }
    return NULL;
}

/* ─── main ─── */
int main(int argc, char **argv)
{
    /* ─ ncurses init ─ */
    initscr(); noecho(); cbreak();

    char username[64], password[64], confirm[64];
    char room[16], pub_pass[32], owner_pass[32];

    mvprintw(2,2,"Welcome to Ninja Chat 🔒");
    mvprintw(4,2,"1. Register");
    mvprintw(5,2,"2. Login");
    mvprintw(6,2,"Choose option: "); refresh();
    echo(); char opt = getch(); noecho(); clear();

    mvprintw(2,2,"Username (no 'ninja-' prefix): ");
    echo(); getnstr(username,50); noecho();
    mvprintw(3,2,"Password: "); echo(); getnstr(password,50); noecho();

    if (opt == '1') {                             /*  register  */
        mvprintw(4,2,"Confirm password: "); echo(); getnstr(confirm,50); noecho();
        if (strcmp(password, confirm)) { mvprintw(6,2,"❌ Mismatch!"); getch(); endwin(); return 0; }
        int ok = register_user(username, password);
        mvprintw(6,2, ok==1 ? "✅ Registered." : "❌ User exists."); getch();
    } else if (opt == '2') {                      /*  login  */
        int ok = login_user(username, password);
        mvprintw(6,2, ok ? "✅ Login OK." : "❌ Wrong creds."); getch();
        if (!ok){ endwin(); return 0; }
    } else { endwin(); return 0; }

    /* clear password from RAM */
    explicit_bzero(password, sizeof(password));
    explicit_bzero(confirm, sizeof(confirm));

    clear();
    mvprintw(2,2,"1-Create room   2-Join"); refresh();
    echo(); char choice = getch(); noecho();
    int is_owner = (choice == '1');

    clear();
    mvprintw(2,2,"Room number: "); echo(); getnstr(room,15); noecho();
    if (is_owner) {
        mvprintw(3,2,"Owner password: "); echo(); getnstr(owner_pass,31); noecho();
        pub_pass[0]='\0';
    } else {
        mvprintw(3,2,"Public password: "); echo(); getnstr(pub_pass,31); noecho();
        owner_pass[0]='\0';
    }

    /* ─ TLS init ─ */
    SSL_library_init(); SSL_load_error_strings(); OpenSSL_add_all_algorithms();
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());

    /* Load & verify CA (assumes cert.pem from server copied as ca.pem) */
    SSL_CTX_load_verify_locations(ctx, "ca.pem", NULL);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv = {.sin_family=AF_INET, .sin_port=htons(PORT)};
    const char *addr = argc>1 ? argv[1] : getenv("NINJA_SERVER");
    if (!addr) addr = "127.0.0.1";
    inet_pton(AF_INET, addr, &srv.sin_addr);
    connect(sockfd, (void *)&srv, sizeof(srv));

    ssl = SSL_new(ctx); SSL_set_fd(ssl, sockfd);
    if (SSL_connect(ssl) <= 0) { ERR_print_errors_fp(stderr); endwin(); return 0; }

    /* send login packet */
    char full_user[64] = "ninja-"; strcat(full_user, username);
    char login[BUFFER_SIZE];
    snprintf(login,sizeof(login),"%s:%s:%s:%s:%d",
             full_user, room, pub_pass, owner_pass, is_owner);
    SSL_write(ssl, login, strlen(login));

    char resp[BUFFER_SIZE]; int n = SSL_read(ssl, resp, sizeof(resp)-1);
    resp[n]='\0';
    if (!strcmp(resp,"WAITING_APPROVAL")) {
        mvprintw(8,2,"Waiting approval…"); refresh();
        n = SSL_read(ssl, resp, sizeof(resp)-1); resp[n]='\0';
    }
    if (strcmp(resp,"APPROVED")) { mvprintw(10,2,"❌ Access denied"); getch(); endwin(); return 0; }

    pthread_t tid; pthread_create(&tid,NULL,receiver,NULL); pthread_detach(tid);

    /* ─ chat loop ─ */
    char msg[BUFFER_SIZE];
    while (1) {
        pthread_mutex_lock(&scr_lock);
        move(16,2); clrtoeol(); echo();
        getnstr(msg, BUFFER_SIZE-1); noecho();
        pthread_mutex_unlock(&scr_lock);
        SSL_write(ssl, msg, strlen(msg));
    }
}
