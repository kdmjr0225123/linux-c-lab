/* 3.1.1 - ask before quitting on Ctrl-C */
#include <signal.h>
#include <string.h>
#include <unistd.h>

void handler(int signum) {
    (void)signum;
    char c;
    const char *msg = "\nDo you really want to quit [y/n]? ";
    write(STDOUT_FILENO, msg, strlen(msg));

    if (read(STDIN_FILENO, &c, 1) == 1 && (c == 'y' || c == 'Y')) {
        write(STDOUT_FILENO, "Bye!\n", 5);
        _exit(0);
    }
    /* clear rest of line */
    while (c != '\n' && read(STDIN_FILENO, &c, 1) == 1)
        ;
    write(STDOUT_FILENO, "Continuing...\n", 14);
    signal(SIGINT, handler);
}

int main(void) {
    signal(SIGINT, handler);
    const char *msg = "Running... press Ctrl-C to try to quit.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    while (1)
        pause();
    return 0;
}
