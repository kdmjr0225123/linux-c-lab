/* Exercise 3 - interactive client, type "exit" to quit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
    while (1) {
        printf("Enter message: ");
        fflush(stdout);
        if (!fgets(buffer, sizeof(buffer), stdin))
            break;
        buffer[strcspn(buffer, "\n")] = '\0';
        send(sock, buffer, strlen(buffer), 0);
        if (strcmp(buffer, "exit") == 0)
            break;
        n = read(sock, buffer, sizeof(buffer) - 1);
        if (n <= 0)
            break;
        buffer[n] = '\0';
        printf("%s\n", buffer);
    }
    close(sock);
    return 0;
}
