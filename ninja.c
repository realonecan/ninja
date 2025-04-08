#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT 55552
#define BUFFER_SIZE 1024
#define MAX_USERNAME 16
#define MAX_ROOM 6
#define MAX_PASS 16

int sockfd;

void* receive_messages(void* arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        int bytes = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            printf("Disconnected\n");
            close(sockfd);
            exit(0);
        }
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }
    return NULL;
}

int main() {
    char username[MAX_USERNAME], room[MAX_ROOM], password[MAX_PASS], choice[2];
    printf("Enter your username: ");
    fgets(username, MAX_USERNAME, stdin);
    username[strcspn(username, "\n")] = 0;

    printf("Do you have a room or want to join one? (1 = Have a room, 2 = Join existing): ");
    fgets(choice, sizeof(choice) + 1, stdin);
    choice[strcspn(choice, "\n")] = 0;

    if (choice[0] == '1') {
        printf("Enter your room number: ");
        fgets(room, MAX_ROOM, stdin);
        room[strcspn(room, "\n")] = 0;
        printf("Enter your owner password: ");
        fgets(password, MAX_PASS, stdin);
        password[strcspn(password, "\n")] = 0;
    } else if (choice[0] == '2') {
        printf("Enter the room number: ");
        fgets(room, MAX_ROOM, stdin);
        room[strcspn(room, "\n")] = 0;
        printf("Enter the room password: ");
        fgets(password, MAX_PASS, stdin);
        password[strcspn(password, "\n")] = 0;
    } else {
        printf("Invalid choice\n");
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    struct hostent* host = gethostbyname("127.0.0.1"); // Replace with VPS IP later
    if (host == NULL) {
        perror("Host resolution failed");
        exit(1);
    }
    server_addr.sin_addr = *((struct in_addr*)host->h_addr);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    // Send username:room:is_owner (no password check here; bot handles it)
    char init_msg[BUFFER_SIZE];
    snprintf(init_msg, sizeof(init_msg), "%s:%s:%d", username, room, choice[0] == '1');
    send(sockfd, init_msg, strlen(init_msg), 0);

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, NULL);
    pthread_detach(recv_thread);

    char buffer[BUFFER_SIZE];
    while (1) {
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) break;
        send(sockfd, buffer, strlen(buffer), 0);
    }

    close(sockfd);
    return 0;
}