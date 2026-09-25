/* 1.1 - Hello World with a thread */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void *identify(void *arg) {
    (void)arg;
    pid_t pid = getpid();
    printf("My pid is %d\n", pid);
    return NULL;
}

int main(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, &identify, NULL);
    identify(NULL);
    return 0;
}
