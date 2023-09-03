#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../../fs/defines.h"

int remove_file(const char *filename) {
    if (unlink(filename) == 0) {
        return 1;
    } else {
        printf("rm error\n", filename);
        return 0;
    }
}

int remove_directory(const char *dirname) {
    if (rmdir(dirname) == 0) {
        return 1;
    } else {
        printf("rm error\n", dirname);
        return 0;
    }
}

int main(int argc, char * argv[]) {
	if (argc != 2) {
		printf("Usage: rm <path>\n");
		return 0;
	}

    struct stat st;
    if (stat(argv[1], &st) == 0) {
        if (S_IFREG == st.st_mode) {
            printf("S_IFREG\n");
        	remove_file(argv[1]);
        }
        else if (S_IFDIR == st.st_mode) {
        	remove_directory(argv[1]);
        }
        else {
        	printf("path %s not exsit\n", argv[1]);
        }
    } else {
        printf("path %s not exsit\n", argv[1]);
    }

    return 0;
}
