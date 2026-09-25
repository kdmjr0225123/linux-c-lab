/*
 * Exercise 7 - broadcast server
 * Parent keeps the list of client sockets. Each child reads from its client
 * and sends the message to the parent through a pipe; the parent writes it
 * to every other client.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 10

struct msg {
    int from;          /* sender's socket fd (same number in parent) */
    int left;          /* 1 = client disconnected */
    char text[256];
};

int clients[MAX_CLIENTS];
int nclients = 0;

void child(int client_sock, int pipe_w) {
    struct msg m;
    int n;
    memset(&m, 0, sizeof(m));
    m.from = client_sock;
    while ((n = read(client_sock, m.text, sizeof(m.text) - 1)) > 0) {
        m.text[n] = '\0';
        m.text[strcspn(m.text, "\r\n")] = '\0';
        if (strcmp(m.text, "exit") == 0)
            break;
        write(pipe_w, &m, sizeof(m));
    }
    m.left = 1;
    write(pipe_w, &m, sizeof(m));
    exit(0);
}

void broadcast(struct msg *m) {
    char out[300];
    snprintf(out, sizeof(out), "Client %d: %s\n", m->from, m->text);
    printf("Broadcasting %s", out);
    for (int i = 0; i < nclients; i++)
        if (clients[i] != m->from)
            write(clients[i], out, strlen(out));
}

void remove_client(int fd) {
    for (int i = 0; i < nclients; i++)
        if (clients[i] == fd) {
            clients[i] = clients[--nclients];
            break;
        }
    close(fd);
    printf("Client %d left (%d connected)\n", fd, nclients);
}

int main() {
    int server_sock, client_sock, opt = 1, p[2];
    struct sockaddr_in server_addr;

    signal(SIGCHLD, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);
    pipe(p);
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);
    printf("Broadcast server listening on port %d\n", PORT);

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_sock, &fds);
        FD_SET(p[0], &fds);
        int maxfd = server_sock > p[0] ? server_sock : p[0];
        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0)
            continue;

        if (FD_ISSET(server_sock, &fds)) {
            client_sock = accept(server_sock, NULL, NULL);
            if (client_sock < 0)
                continue;
            if (nclients == MAX_CLIENTS) {
                close(client_sock);
                continue;
            }
            clients[nclients++] = client_sock;
            printf("Client %d joined (%d connected)\n", client_sock, nclients);
            if (fork() == 0) {
                close(server_sock);
                close(p[0]);
                child(client_sock, p[1]);
            }
        }
        if (FD_ISSET(p[0], &fds)) {
            struct msg m;
            if (read(p[0], &m, sizeof(m)) == sizeof(m)) {
                if (m.left)
                    remove_client(m.from);
                else
                    broadcast(&m);
            }
        }
    }
    return 0;
}
