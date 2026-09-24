/*
 * 4.2.1 Warmup - What does C print?
 * Terminal:        Luke, I am your...
 * output_file.txt: father
 * The first printf goes to the terminal (stdout is line-buffered on a tty,
 * so "\n" flushes it). dup2(newfd, 1) then makes fd 1 point to the file,
 * so "father" lands in output_file.txt.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int newfd;
    if ((newfd = open("output_file.txt", O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
        exit(1);
    }
    printf("Luke, I am your...\n");
    dup2(newfd, 1);
    printf("father\n");
    exit(0);
}
