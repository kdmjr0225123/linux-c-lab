/* 3.1 - no join: output depends on scheduling */
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

void *helper(void *arg) {
    (void)arg;
    printf("HELPER\n");
    return NULL;
}

int main(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, &helper, NULL);
    sched_yield();   /* pthread_yield() is deprecated in glibc; same behavior */
    printf("MAIN\n");
    return 0;
}
