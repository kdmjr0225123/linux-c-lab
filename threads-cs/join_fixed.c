/* 3.1 - join first so HELPER always prints before MAIN */
#include <pthread.h>
#include <stdio.h>

void *helper(void *arg) {
    (void)arg;
    printf("HELPER\n");
    return NULL;
}

int main(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, &helper, NULL);
    pthread_join(thread, NULL);
    printf("MAIN\n");
    return 0;
}
