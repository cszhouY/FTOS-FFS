#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fs/defines.h>
#include <assert.h>


int main(int argc, char const *argv[])
{
	int fd = open("testbuf", O_WRONLY | O_CREAT);
	if (fd < 0) {
		printf("open error\n");
		return 0;
	}
	close(fd);

	char buf[BLOCK_SIZE] = {'0'};
	int start, end;
	const size_t tmp = INODE_NUM_INDIRECT / 8;
	unsigned usetime[tmp + 1];


	for (size_t i = 0; i <= 8; ++i) {
		fd = open("testbuf", O_WRONLY);
		assert(fd > 0);
		size_t fsize = INODE_NUM_DIRECT + tmp * i ;
		start = syscall(228);
		for (size_t j = 0; j < fsize; ++j) {
			write(fd, buf, BLOCK_SIZE);
		}
		end = syscall(228);
		close(fd);
		usetime[i - 1] = (unsigned)(end - start);
		printf("testcase %d, write %u x 512 bytes, cost %u ms\n", i + 1, fsize, usetime[i-1]);
	}

	// syscall(228);
	return 0;
}