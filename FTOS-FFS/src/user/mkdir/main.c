#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
    int i;

    if (argc < 2)
    {
        printf("Usage: mkdir files...\n");
        return 0;
    }

    for (i = 1; i < argc; i++)
    {
        if (mkdir(argv[i], 0000) < 0)
        {
            printf("mkdir: %s failed to create\n", argv[i]);
            break;
        }
    }

    return 0;
}