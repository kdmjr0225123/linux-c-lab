/* Exercise 3 - each child handles messages until "exit" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

#define PORT 8080

void handle_client(int client_sock) {
    char buffer[1024], reply[1100];
    int n;
    while ((n = read(client_sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = '\0';
        printf("[pid %d] Received: %s\n", getpid(), buffer);
        fflush(stdout);
        if (strcmp(buffer, "exit") == 0)
            break;
        snprintf(reply, sizeof(reply), "Echo: %s", buffer);
        write(client_sock, reply, strlen(reply));
    }
    printf("[pid %d] Client disconnected\n", getpid());
    close(client_sock);
}

int main() {
    int server_sock, client_sock, opt = 1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    signal(SIGCHLD, SIG_IGN);
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);
    printf("Server listening on port %d\n", PORT);
    fflush(stdout);

    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (fork() == 0) {
            close(server_sock);
            handle_client(client_sock);
            exit(0);
        }
        close(client_sock);
    }
    return 0;
}
