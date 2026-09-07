#include <stdio.h>

int recur(int i) {
    /* A stack allocated variable within a recursive function */
    int j = i;
    printf("recur call %i: stack@ %p\n", i, (void *)&j);

    if (i > 0) {
        return recur(i - 1);
    }

    return 0;
}
