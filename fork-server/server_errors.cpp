// Exercise 6 - error checking, errors logged to server_errors.log
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <netinet/in.h>
#include <csignal>
#include <cstring>
#include <cerrno>
#include <ctime>

#define PORT 8080

void log_error(const std::string &what) {
    std::ofstream log("server_errors.log", std::ios::app);
    char stamp[32];
    time_t now = time(nullptr);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    log << "[" << stamp << "] " << what << ": " << strerror(errno) << std::endl;
}

void handle_client(int client_sock) {
    char buffer[1024] = {0};
    if (read(client_sock, buffer, sizeof(buffer) - 1) < 0) {
        log_error("read failed");
        close(client_sock);
        return;
    }
    std::cout << "Received: " << buffer << std::endl;
    const char *msg = "Hello from C++ server";
    if (write(client_sock, msg, strlen(msg)) < 0)
        log_error("write failed");
    close(client_sock);
}

int main() {
    int server_sock, client_sock, opt = 1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    signal(SIGCHLD, SIG_IGN);
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_error("socket failed");
        return 1;
    }
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("bind failed");
        return 1;
    }
    if (listen(server_sock, 5) < 0) {
        log_error("listen failed");
        return 1;
    }
    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) {
            log_error("accept failed");
            continue;               // keep serving other clients
        }
        pid_t pid = fork();
        if (pid < 0) {
            log_error("fork failed");
            close(client_sock);
            continue;
        }
        if (pid == 0) {
            close(server_sock);
            handle_client(client_sock);
            exit(0);
        }
        close(client_sock);
    }
    return 0;
}
