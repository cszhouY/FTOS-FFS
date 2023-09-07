#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fs/defines.h>
#include <assert.h>

int fd;
char buf[BLOCK_SIZE];

int init_env(){
	fd = open("testbuf", O_WRONLY | O_CREAT);
	if (fd < 0) {
		printf("open error\n");
		return 0;
	}
	close(fd);
	memset(buf, '0', sizeof(buf));
	return 1;
}

void test_write_single_file() {
	int start, end;

	printf("===== test write single file =====\n");
	const size_t tmp = INODE_NUM_INDIRECT / 8;
	unsigned usetime[9];
	double avgv = 0;

	for (size_t i = 0; i <= 8; ++i) {
		fd = open("testbuf", O_WRONLY);
		assert(fd > 0);
		size_t fsize = INODE_NUM_DIRECT + i * tmp;
		start = syscall(228);
		for (size_t j = 0; j < fsize; ++j) {
			write(fd, buf, BLOCK_SIZE);
		}
		end = syscall(228);
		close(fd);
		usetime[i - 1] = (unsigned)(end - start);
		printf("testcase %d, write %u blocks, cost %u ms\n", i + 1, fsize, usetime[i-1]);
		avgv += (double)fsize / usetime[i-1]; 
	}
	avgv /= 9.0;
	printf("avg write speed %f blk/ms\n", avgv);
}

void test_write_large_file() {
	int start, end;
	double avgv = 0;
	printf("===== test write large file =====\n");
	for (int i = 0; i < 10; ++i) {
		fd = open("testbuf", O_WRONLY);
		assert(fd > 0);
		size_t fsize = INODE_NUM_DIRECT + INODE_NUM_INDIRECT;
		start = syscall(228);
		for (size_t j = 0; j < fsize; ++j) {
			write(fd, buf, BLOCK_SIZE);
		}
		end = syscall(228);
		close(fd);
		printf("testcase %d, write %u blocks, cost %u ms\n", i + 1, fsize, (unsigned)(end - start));
		avgv += (double)fsize / (unsigned)(end - start) / 10;
	}
	printf("avg write speed %f blk/ms\n", avgv);
}

void test_multi_files() {
	int start, end;
	double avg = 0;
	printf ("===== test multiply files =====\n");
	const int nfile = 10;
	int fds[nfile];
	char filename[nfile];
	for(int i = 0; i < nfile; ++i) {
		char tmp[1] = {'0' + i};
		fds[i] = open(tmp, O_WRONLY | O_CREAT);
		assert(fds[i] > 0);
		close(fds[i]);
	}
	int loop = 10;
	while(loop--) {
		unsigned t = 0;
		for(int i = 0; i < nfile; ++i) {
			char tmp[1] = {'0' + i};
			fds[i] = open(tmp, O_WRONLY);
			assert(fds[i] > 0);
			start = syscall(228);
			for (int _ = 0; _ < INODE_NUM_DIRECT; ++_) {
				write(fds[i], buf, BLOCK_SIZE);
			}
			end = syscall(228);
			t += end - start;
			close(fds[i]);
		}
		printf("testcase %d, write %d different files at the same derecotry, cost %u ms\n", 10 - loop, nfile, t);
		avg += (double) t / 10;
	}
	printf("avg write speed %f ms\n", avg);
}

int main(int argc, char const *argv[])
{
	init_env();
	
	// test_write_single_file();

	// test_write_large_file();

	test_multi_files();

	return 0;
}