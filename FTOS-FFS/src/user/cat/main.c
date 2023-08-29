#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

char buf[512];

void cat(int fd)
{
    int n;

    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(1, buf, n);
    if (n < 0)
    {
        printf("cat: read error\n");
        return;
    }
}

int main(int argc, char *argv[])
{
    int fd, i;

    if (argc <= 1)
    {
        printf("Usage: cat files...\n");
        return 0;
    }

    for (i = 1; i < argc; i++)
    {
        if ((fd = open(argv[i], 0000)) < 0)
        {
            printf("cat: cannot open %s\n", argv[i]);
            return 0;
        }
        cat(fd);
        close(fd);
    }
    return 0;
}