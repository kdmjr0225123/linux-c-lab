/* Exercise 1 - basic fork server (C) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

#define PORT 8080

void handle_client(int client_sock) {
    char buffer[1024] = {0};
    read(client_sock, buffer, sizeof(buffer) - 1);
    printf("Received: %s\n", buffer);
    write(client_sock, "Hello from server", 17);
    close(client_sock);
}

int main() {
    int server_sock, client_sock, opt = 1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    signal(SIGCHLD, SIG_IGN);   /* reap children automatically */
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
        close(client_sock);     /* parent doesn't need it */
    }
    return 0;
}
