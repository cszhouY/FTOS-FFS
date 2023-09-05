#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
// #include <aarch64/intrinsic.h>
#include <common/defines.h>

#include "../../fs/defines.h"
#include "../../fs/inode.h"

u64 get_timestamp() {
    u64 result;
    asm volatile("mrs %0, cntvct_el0" : "=r" (result));
    return result;
}


int main(int argc, char const *argv[])
{
	int fd = open("testbuf", O_WRONLY | O_CREAT);
	if (fd < 0) {
		printf("open error\n");
		return 0;
	}
	printf("open\n");
	u64 start, end;
	printf("clock\n");
	size_t fsize = BLOCK_SIZE / 2;

	start = get_timestamp();
	char ch = '0';
	for (size_t i = 0; i < fsize; ++i) {
		write(fd, &ch, 1);
	}
	end = get_timestamp();
	printf("write %u bits, use time %u \n", fsize, (end - start));
	close(fd);
	return 0;
}