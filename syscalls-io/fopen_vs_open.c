/*
 * 3.2.1 Files vs File Descriptor - fopen vs open
 * open()  : system call, returns an int file descriptor, unbuffered,
 *           used with read()/write()/lseek(), POSIX-only.
 * fopen() : C standard library call, returns a FILE* stream that wraps a
 *           file descriptor and adds a user-space buffer, used with
 *           fprintf()/fgets()/fread(), portable to any C platform.
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    /* open(): raw syscall, returns an int */
    int fd = open("open_demo.txt", O_CREAT | O_TRUNC | O_WRONLY, 0644);
    const char *s = "written with write() on fd\n";
    write(fd, s, strlen(s));
    printf("open()  returned file descriptor: %d\n", fd);
    close(fd);

    /* fopen(): library call, returns FILE* (buffered) */
    FILE *fp = fopen("fopen_demo.txt", "w");
    fprintf(fp, "written with fprintf() on FILE*\n");
    printf("fopen() returned FILE* %p, underlying fd = fileno(fp) = %d\n",
           (void *)fp, fileno(fp));
    fclose(fp);   /* flushes the user-space buffer, then close(fd) */
    return 0;
}
