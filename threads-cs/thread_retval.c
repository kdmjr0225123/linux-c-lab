/* 3.4 - get worker's return value with pthread_join */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

void *worker(void *arg) {
    int *data = (int *) arg;
    *data = *data + 1;
    printf("Data is %d\n", *data);
    return (void *) 42;
}

int main(void) {
    int data = 0;
    void *ret;
    pthread_t thread;
    pthread_create(&thread, NULL, &worker, &data);
    pthread_join(thread, &ret);
    printf("worker returned %d\n", (int)(intptr_t) ret);
    return 0;
}
