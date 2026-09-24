/*
 * 4.1.1 Quick practice with write and seek
 * Result: test.txt is 700 bytes:
 *   bytes   0-199 : 'a'   (first write)
 *   bytes 200-599 : '\0'  (gap: offset 600 is past old EOF of 200)
 *   bytes 600-699 : 'a'   (second write of 100 bytes)
 * (Mode 0644 added: open with O_CREAT requires a third argument.)
 */
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char buffer[200];
    memset(buffer, 'a', 200);
    int fd = open("test.txt", O_CREAT | O_RDWR, 0644);
    write(fd, buffer, 200);           /* offset 0 -> 200            */
    lseek(fd, 0, SEEK_SET);           /* offset -> 0                */
    read(fd, buffer, 100);            /* offset 0 -> 100            */
    lseek(fd, 500, SEEK_CUR);         /* offset 100 -> 600          */
    write(fd, buffer, 100);           /* writes bytes 600-699       */
    close(fd);
    return 0;
}
