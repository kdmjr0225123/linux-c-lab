/* Exercise 5 - child closes the connection after 10s of silence (select) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <arpa/inet.h>

#define PORT 8080
#define TIMEOUT 10

void handle_client(int client_sock) {
    char buffer[1024] = {0};
    fd_set fds;
    struct timeval tv = { TIMEOUT, 0 };

    FD_ZERO(&fds);
    FD_SET(client_sock, &fds);
    if (select(client_sock + 1, &fds, NULL, NULL, &tv) == 0) {
        printf("[pid %d] No message in %d seconds, closing connection\n", getpid(), TIMEOUT);
        close(client_sock);
        return;
    }
    read(client_sock, buffer, sizeof(buffer) - 1);
    printf("[pid %d] Received: %s\n", getpid(), buffer);
    write(client_sock, "Hello from server", 17);
    close(client_sock);
}

int main() {
    int server_sock, client_sock, opt = 1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    signal(SIGCHLD, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);
    printf("Timeout server listening on port %d\n", PORT);

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
