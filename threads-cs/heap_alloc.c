/* 3.3 - thread writes to a heap buffer */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *helper(void *arg) {
    char *message = (char *) arg;
    strcpy(message, "I am the child");
    return NULL;
}

int main(void) {
    char *message = malloc(100);
    strcpy(message, "I am the parent");
    pthread_t thread;
    pthread_create(&thread, NULL, &helper, message);
    pthread_join(thread, NULL);
    printf("%s\n", message);
    free(message);
    return 0;
}
