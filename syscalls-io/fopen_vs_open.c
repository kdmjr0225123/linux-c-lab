/* 3.2.1 - open() vs fopen() */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    int fd = open("open_demo.txt", O_CREAT | O_TRUNC | O_WRONLY, 0644);
    const char *s = "written with write() on fd\n";
    write(fd, s, strlen(s));
    printf("open()  returned file descriptor: %d\n", fd);
    close(fd);

    FILE *fp = fopen("fopen_demo.txt", "w");
    fprintf(fp, "written with fprintf() on FILE*\n");
    printf("fopen() returned FILE* %p, underlying fd = fileno(fp) = %d\n",
           (void *)fp, fileno(fp));
    fclose(fp);
    return 0;
}
