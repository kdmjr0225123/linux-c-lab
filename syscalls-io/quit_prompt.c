/*
 * 3.1.1 Did you really want to quit?
 * On Ctrl-C, ask "Do you really want to quit [y/n]? ".
 * 'y' -> exit, anything else -> keep running.
 * Uses only syscalls (write/read/_exit) inside the handler,
 * since printf/scanf are not async-signal-safe.
 */
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
    /* discard rest of the line (e.g. the newline) */
    while (c != '\n' && read(STDIN_FILENO, &c, 1) == 1)
        ;
    write(STDOUT_FILENO, "Continuing...\n", 14);
    signal(SIGINT, handler);          /* re-install (portable) */
}

int main(void) {
    signal(SIGINT, handler);
    const char *msg = "Running... press Ctrl-C to try to quit.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    while (1)
        pause();                      /* sleep until a signal arrives */
    return 0;
}
