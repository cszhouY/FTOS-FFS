#include <stdio.h>

int main(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++)
        // 对于echo中带有空格的部分，判断是否为文本尾
        // 由此决定打印空格还是换行符
        printf("%s%s", argv[i], i + 1 < argc ? " " : "\n");
    
    return 0;
}