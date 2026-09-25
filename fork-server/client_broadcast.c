/* Exercise 7 - client: child prints broadcasts, parent sends what you type */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int sock, n;
    struct sockaddr_in server_addr;
    char buffer[1024];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    pid_t pid = fork();
    if (pid == 0) {                       /* receive loop */
        while ((n = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        }
        exit(0);
    }
    while (fgets(buffer, sizeof(buffer), stdin)) {   /* send loop */
        send(sock, buffer, strlen(buffer), 0);
        if (strncmp(buffer, "exit", 4) == 0)
            break;
    }
    sleep(1);
    kill(pid, SIGTERM);
    close(sock);
    return 0;
}
