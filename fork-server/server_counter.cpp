// Exercise 4 - count active clients in shared memory (shmget)
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <csignal>
#include <cstring>

#define PORT 8080

int *active;   // lives in shared memory, seen by every child

void handle_client(int client_sock) {
    int count = __sync_add_and_fetch(active, 1);
    std::cout << "[pid " << getpid() << "] New client. Active clients: " << count << std::endl;

    char buffer[1024];
    int n;
    while ((n = read(client_sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (strcmp(buffer, "exit") == 0)
            break;
        std::string reply = std::string("Echo: ") + buffer;
        write(client_sock, reply.c_str(), reply.size());
    }
    close(client_sock);
    count = __sync_sub_and_fetch(active, 1);
    std::cout << "[pid " << getpid() << "] Client left. Active clients: " << count << std::endl;
}

int main() {
    int server_sock, client_sock, opt = 1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    int shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600);
    active = (int *) shmat(shmid, nullptr, 0);
    *active = 0;
    shmctl(shmid, IPC_RMID, nullptr);   // freed automatically when all processes detach

    signal(SIGCHLD, SIG_IGN);
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);
    std::cout << "Counter server listening on port " << PORT << std::endl;

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
