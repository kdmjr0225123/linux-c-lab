/*
 * 4.2.2 Redirection: executing a process after dup2
 * Prints: writing output of the command /bin/ls to "<file>"  (terminal)
 * Then dup2 points stdout at <file> and execvp replaces this process with
 * `ls -al /`. ls inherits fd 1, so its listing goes into <file>.
 * perror/exit only run if execvp fails.
 * (Fix from handout: O_CAT -> O_CREAT.)
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int newfd;
    char *cmd[] = { "/bin/ls", "-al", "/", 0 };
    if (argc != 2) {
        fprintf(stderr, "usage: %s output_file\n", argv[0]);
        exit(1);
    }
    if ((newfd = open(argv[1], O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
        perror(argv[1]);   /* open failed */
        exit(1);
    }
    printf("writing output of the command %s to \"%s\"\n", cmd[0], argv[1]);
    fflush(stdout);        /* make sure the message is out before exec */
    dup2(newfd, 1);
    execvp(cmd[0], cmd);
    perror(cmd[0]);        /* execvp failed */
    exit(1);
}
