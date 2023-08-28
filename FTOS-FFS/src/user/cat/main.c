#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printf("Failed to open file: %s\n", filename);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, BUFFER_SIZE)) > 0) {
        ssize_t bytesWritten = write(STDOUT_FILENO, buffer, bytesRead);
        if (bytesWritten != bytesRead) {
            printf("Failed to write to stdout\n");
            close(fd);
            return 1;
        }
    }

    if (bytesRead == -1) {
        printf("Failed to read file: %s\n", filename);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}