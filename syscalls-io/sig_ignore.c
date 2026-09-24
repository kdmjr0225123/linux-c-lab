/*
 * 3.1 Warmup - How do we stop this program?
 * SIGINT (Ctrl-C) is ignored, so Ctrl-C does nothing.
 * Ways to stop it:
 *   - Ctrl-\  sends SIGQUIT (not ignored -> core dump / quit)
 *   - Ctrl-Z  then `kill %1` (SIGTERM) or `kill -9 %1`
 *   - From another terminal: kill -9 <pid>  (SIGKILL can never be caught or ignored)
 */
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
