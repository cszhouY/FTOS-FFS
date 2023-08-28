#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

// 比较函数，用于qsort排序
int compare(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

int main(int argc, char *argv[]) {
    struct dirent *entry;
    DIR *dp;
    struct stat file_info;

    // 默认列出当前目录
    char *path = ".";

    // 如果提供了命令行参数，则使用第一个参数作为路径
    if (argc > 1) {
        path = argv[1];
    }

    dp = opendir(path);

    if (dp == NULL) {
        perror("opendir");
        return 1;
    }

    // 保存目录项名字的数组
    char *entries[1000];
    int count = 0;

    while ((entry = readdir(dp))) {
        entries[count++] = strdup(entry->d_name);
    }

    // 使用qsort对目录项进行排序
    qsort(entries, count, sizeof(char *), compare);

    for (int i = 0; i < count; i++) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entries[i]);

        // 获取文件信息
        if (stat(full_path, &file_info) == -1) {
            perror("stat");
            continue;
        }

        printf("%s", entries[i]);

        // 如果是目录，则显示一个斜杠
        if (S_ISDIR(file_info.st_mode)) {
            printf("/");
        }

        printf("\n");
    }

    // 释放分配的内存
    for (int i = 0; i < count; i++) {
        free(entries[i]);
    }

    closedir(dp);

    return 0;
}
