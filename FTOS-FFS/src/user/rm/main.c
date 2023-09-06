#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../../fs/defines.h"

// 自定义函数，通过87号系统调用进行删除操作
int myunlink(const char *filename) {
    return syscall(87, filename);
}

int main(int argc, char * argv[]) {
    if (argc != 2) {
        printf("Usage: rm <path>\n");
        return 0;
    }

    struct stat st;

    // 对应文件不存在或者系统调用失败的情况
    if (!(stat(argv[1], &st) == 0 && myunlink(argv[1]) == 0))
        printf("rm %s failed\n", argv[1]);

    return 0;
}
