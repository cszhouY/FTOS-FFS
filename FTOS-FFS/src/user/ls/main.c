#include "../../fs/defines.h"
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

char *fmtname(char *path)
{
    static char buf[FILE_NAME_MAX_LENGTH + 1];
    char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // Return blank-padded name.
    if (strlen(p) >= FILE_NAME_MAX_LENGTH)
        return p;
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), ' ', FILE_NAME_MAX_LENGTH - strlen(p));
    return buf;
}

void ls(char *path)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        printf("ls: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        printf("ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.st_mode)
    {
    case S_IFREG:
        printf("%s %d %d %d\n", fmtname(path), st.st_mode, st.st_ino, st.st_size);
        break;

    case S_IFDIR:
        if (strlen(path) + 1 + FILE_NAME_MAX_LENGTH + 1 > sizeof buf)
        {
            printf("ls: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inode_no == 0)
                continue;
            memmove(p, de.name, FILE_NAME_MAX_LENGTH);
            p[FILE_NAME_MAX_LENGTH] = 0;
            if (stat(buf, &st) < 0)
            {
                printf("ls: cannot stat %s\n", buf);
                continue;
            }
            printf("%s %d %d %d\n", fmtname(buf), st.st_mode, st.st_ino, st.st_size);
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    int i;

    if (argc < 2)
    {
        ls(".");
        return 0;
    }
    for (i = 1; i < argc; i++)
        ls(argv[i]);
    
    
    return 0;
}