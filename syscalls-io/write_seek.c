/* 4.1.1 - write and lseek practice */
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char buffer[200];
    memset(buffer, 'a', 200);
    int fd = open("test.txt", O_CREAT | O_RDWR, 0644);
    write(fd, buffer, 200);
    lseek(fd, 0, SEEK_SET);
    read(fd, buffer, 100);
    lseek(fd, 500, SEEK_CUR);
    write(fd, buffer, 100);
    close(fd);
    return 0;
}
