#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

char buf[512];

void cat(int fd)
{
    int n;

    // 循环读取fd中的内容直到文本尾
    // 每次读取都将缓存内容写入（打印）到控制台中
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(1, buf, n); // 1为标准输出流
    // 读取失败
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

    // 对于多个对象，cat依次打印文本内容
    for (i = 1; i < argc; i++)
    {
        // FTOS限制，打开标签必须为0000
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