// Exercise 2 - basic fork server (C++)
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <csignal>
#include <cstring>

#define PORT 8080

void handle_client(int client_sock) {
    char buffer[1024] = {0};
    read(client_sock, buffer, sizeof(buffer) - 1);
    std::cout << "Received: " << buffer << std::endl;
    const char *msg = "Hello from C++ server";
    write(client_sock, msg, strlen(msg));
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
    std::cout << "C++ server listening on port " << PORT << std::endl;

    while (true) {
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
