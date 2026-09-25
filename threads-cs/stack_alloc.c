/* 3.2 - thread writes to main's stack variable */
#include <pthread.h>
#include <stdio.h>

void *helper(void *arg) {
    int *num = (int *) arg;
    *num = 2;
    return NULL;
}

int main(void) {
    int i = 0;
    pthread_t thread;
    pthread_create(&thread, NULL, &helper, &i);
    pthread_join(thread, NULL);
    printf("i is %d\n", i);
    return 0;
}
