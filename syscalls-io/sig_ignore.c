/* 3.1 - ignores Ctrl-C (SIGINT) */
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
    signal(SIGINT, SIG_IGN);
    printf("PID %d running; Ctrl-C is ignored.\n", getpid());
    fflush(stdout);
    while (1)
        ;
}
