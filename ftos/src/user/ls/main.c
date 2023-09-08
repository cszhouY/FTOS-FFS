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

    // 完整路径从后向前遍历，直到遇到第一个‘/’
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // 文件名足够长，不需要填充空格
    if (strlen(p) >= FILE_NAME_MAX_LENGTH)
        return p;
    
    // 否则使用空格填充文件名并返回
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), ' ', FILE_NAME_MAX_LENGTH - strlen(p));
    return buf;
}

// 核心程序，遍历path下的所有文件以及子目录并打印
void ls(char *path)
{
    char buf[512], *p;
    // 文件描述符
    int fd;
    // 目录项，用于遍历目录下文件及子目录
    struct dirent de;
    // 用于存储文件状态，便于打印
    struct stat st;

    // 文件打开失败
    if ((fd = open(path, 0)) < 0)
    {
        printf("ls: cannot open %s\n", path);
        return;
    }
    // 获取文件状态信息失败
    if (fstat(fd, &st) < 0)
    {
        printf("ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 根据文件状态信息中的“st_mode”字段判断当前path对应的文件类型
    switch (st.st_mode)
    {
    // 通常文件，直接打印相关信息即可
    case S_IFREG:
        printf("%s %d %d %d\n", fmtname(path), st.st_mode, st.st_ino, st.st_size);
        break;
    // 目录文件，需要打印目录下的所有文件和子目录
    case S_IFDIR:
        // 完整路径过长，buf无法存储
        if (strlen(path) + 1 + FILE_NAME_MAX_LENGTH + 1 > sizeof buf)
        {
            printf("ls: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        // 手动添加最后一个‘/’，后续在‘/’后接上目录下的文件名
        *p++ = '/';
        // 循环将目录信息读取到de的inode_no以及name中
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            // 表明目录为空
            if (de.inode_no == 0)
                continue;
            memmove(p, de.name, FILE_NAME_MAX_LENGTH);
            p[FILE_NAME_MAX_LENGTH] = 0;
            // 无法获取文件信息
            if (stat(buf, &st) < 0)
            {
                printf("ls: cannot stat %s\n", buf);
                continue;
            }
            // 打印相关信息
            printf("%s %d %d %d\n", fmtname(buf), st.st_mode, st.st_ino, st.st_size);
        }
        break;
    }
    close(fd);
}

// 主程序
int main(int argc, char *argv[])
{
    int i;

    // ls如果没有指定路径则默认为当前路径
    if (argc < 2)
    {
        ls(".");
        return 0;
    }
    // 将待遍历路径循环传入ls中并调用
    for (i = 1; i < argc; i++)
        ls(argv[i]);
    
    return 0;
}   