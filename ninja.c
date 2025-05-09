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

#define PORT 55555
#define BUFFER_SIZE 1024

SSL* ssl;
int sockfd;

void* receive_messages(void* arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        mvprintw(14, 2, "%s", buffer);
        refresh();
    }
    return NULL;
}

int main() {
    char username[64], password[64], confirm[64];
    char room[16], public_pass[32], owner_pass[32];

    // Init ncurses
    initscr();
    noecho();
    cbreak();

    mvprintw(2, 2, "Welcome to Ninja Chat 🔒");
    mvprintw(4, 2, "1. Register");
    mvprintw(5, 2, "2. Login");
    mvprintw(6, 2, "Choose option: ");
    refresh();

    echo();
    char opt = getch();
    noecho();
    clear();

    mvprintw(2, 2, "Username (no 'ninja-' prefix): ");
    echo(); getnstr(username, 50); noecho();

    mvprintw(3, 2, "Password: ");
    echo(); getnstr(password, 50); noecho();

    if (opt == '1') {
        mvprintw(4, 2, "Confirm password: ");
        echo(); getnstr(confirm, 50); noecho();

        if (strcmp(password, confirm) != 0) {
            mvprintw(6, 2, "❌ Passwords do not match! Press any key to continue...");
            getch(); endwin(); return 1;
        }

        if (register_user(username, password)) {
            mvprintw(6, 2, "✅ Registered successfully! Press any key to continue...");
        } else {
            mvprintw(6, 2, "❌ Username already exists! Press any key to continue...");
            getch(); endwin(); return 1;
        }
    } else if (opt == '2') {
        if (!login_user(username, password)) {
            mvprintw(6, 2, "❌ Invalid credentials! Press any key to continue...");
            getch(); endwin(); return 1;
        }
        mvprintw(6, 2, "✅ Login successful! Press any key to continue...");
    } else {
        mvprintw(6, 2, "🚧 Feature under development. Press any key to continue...");
        getch(); endwin(); return 1;
    }

    getch(); clear();

    mvprintw(2, 2, "Do you want to:");
    mvprintw(3, 4, "1 - Create a Room");
    mvprintw(4, 4, "2 - Join Existing Room");
    mvprintw(5, 2, "Enter choice: ");
    refresh();
    echo();
    char choice = getch();
    noecho();
    int is_owner = (choice == '1');

    clear();
    mvprintw(2, 2, "Room number: ");
    echo(); getnstr(room, 15); noecho();

    if (is_owner) {
        mvprintw(3, 2, "Set room owner password: ");
        echo(); getnstr(owner_pass, 31); noecho();
        strcpy(public_pass, "");
    } else {
        mvprintw(3, 2, "Enter room public password: ");
        echo(); getnstr(public_pass, 31); noecho();
        strcpy(owner_pass, "");
    }

    char full_username[64] = "ninja-";
    strcat(full_username, username);

    // === Initialize SSL ===
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };
    inet_pton(AF_INET, "16.171.198.136", &server_addr.sin_addr);

    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    SSL_connect(ssl);

    // Send formatted login info
    char login_data[BUFFER_SIZE];
    snprintf(login_data, sizeof(login_data), "%s:%s:%s:%s:%d", 
             full_username, room, public_pass, owner_pass, is_owner);
    SSL_write(ssl, login_data, strlen(login_data));

    char response[BUFFER_SIZE];
    int bytes = SSL_read(ssl, response, sizeof(response) - 1);
    response[bytes] = '\0';

    if (strcmp(response, "WAITING_APPROVAL") == 0) {
        mvprintw(8, 2, "Waiting for room owner approval..."); refresh();
        bytes = SSL_read(ssl, response, sizeof(response) - 1);
        response[bytes] = '\0';
    }

    if (strcmp(response, "APPROVED") == 0) {
        mvprintw(10, 2, "✅ Approved! You can now chat.");
    } else {
        mvprintw(10, 2, "❌ Access denied.");
        refresh();
        getch(); endwin(); return 0;
    }

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, NULL);
    pthread_detach(recv_thread);

    char msg[BUFFER_SIZE];
    while (1) {
        move(16, 2); clrtoeol(); echo();
        getnstr(msg, BUFFER_SIZE - 1); noecho();
        SSL_write(ssl, msg, strlen(msg));
    }

    endwin();
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
    return 0;
}
