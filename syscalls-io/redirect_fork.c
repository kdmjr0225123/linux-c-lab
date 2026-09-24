/* 4.2.3 - redirect in child, parent waits */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int pid, status;
    int newfd;
    char *cmd[] = { "/bin/ls", "-al", "/", 0 };
    if (argc != 2) {
        fprintf(stderr, "usage: %s output_file\n", argv[0]);
        exit(1);
    }
    if ((newfd = open(argv[1], O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
        perror(argv[1]);
        exit(1);
    }
    printf("writing output of the command %s to \"%s\"\n", cmd[0], argv[1]);
    fflush(stdout);

    if ((pid = fork()) < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {   /* child */
        dup2(newfd, 1);
        close(newfd);
        execvp(cmd[0], cmd);
        perror(cmd[0]);
        exit(1);
    }
    /* parent */
    close(newfd);
    waitpid(pid, &status, 0);
    printf("all done\n");
    exit(0);
}
