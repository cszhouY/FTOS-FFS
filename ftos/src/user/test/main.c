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
	int fsize = INODE_NUM_DIRECT + INODE_NUM_INDIRECT;
	int start, end;
	double avg = 0;
	printf ("===== test multiply files =====\n");
	const int nfile = 10;
	char *filename[10] = {"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10"};
	for(int i = 0; i < nfile; ++i) {
		fd = open(filename[i], O_WRONLY | O_CREAT);
		assert(fd > 0);
		close(fd);
	}
	int ncase = 10;
	for (int c = 1; c <= ncase; ++c) {
		unsigned t = 0;
		for(int i = 0; i < nfile; ++i) {
			fd = open(filename[i], O_WRONLY);
			assert(fd > 0);
			start = syscall(228);
			for (int _ = 0; _ < fsize; ++_) {
				write(fd, buf, BLOCK_SIZE);
			}
			end = syscall(228);
			t += end - start;
			close(fd);
		}
		printf("testcase %d, write %d different files at the same derecotry, cost %u ms\n", c, nfile, t);
		avg += (double) t / 10;
	}
	printf("avg write time %f ms\n", avg);

	avg = 0;
	for (int c = 1; c <= ncase; ++c) {
		unsigned t = 0;
		for(int i = 0; i < nfile; ++i) {
			fd = open(filename[i], O_RDONLY);
			assert(fd > 0);
			start = syscall(228);
			for (int _ = 0; _ < fsize; ++_) {
				read(fd, buf, BLOCK_SIZE);
			}
			end = syscall(228);
			t += end - start;
			close(fd);
		}
		printf("testcase %d, read %d different files at the same derecotry, cost %u ms\n", c, nfile, t);
		avg += (double) t / 10;
	}
	printf("avg read time %f ms\n", avg);
}

void test_rw_multi_dir() {
	char *dirname[5] = {"d1", "d2", "d3", "d4", "d5"};
	char *filename[10] = {"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10"};
	for (int i = 0; i < 5; ++i) {
		if(mkdir(dirname[i], 0000) < 0) {
			printf("mkdir %s error!\n", dirname[i]);
			exit(1);
		}
	}
	for(int i = 0; i < 10; ++i) {
		for (int j = 0; j < 5; ++j) {
			if (chdir(dirname[j]) < 0) {
				printf("chdir %s error!\n", dirname[j]);
				exit(1);
			}
			fd = open(filename[i], O_WRONLY | O_CREAT);
			assert(fd > 0);
			for (int k = 0; k < INODE_NUM_DIRECT * 4; ++k) {
				write(fd, buf, BLOCK_SIZE);
			}
			close(fd);
			if (chdir("/") < 0) {
				printf("chdir / error!\n");
				exit(1);
			}
		}
	}

	int start, end;
	double t = 0;
	start = syscall(228);
	for(int i = 0; i < 5; ++i) {
		for (int j = 0; j < 10; ++j) {
			if (chdir(dirname[i]) < 0) {       
				printf("chdir %s error!\n", dirname[i]);
				exit(1);
			}
			fd = open(filename[j], O_RDONLY);
			assert(fd > 0);
			for (int k = 0; k < INODE_NUM_DIRECT * 4; ++k) {
				read(fd, buf, BLOCK_SIZE);
			}
			close(fd);
			if (chdir("/") < 0) {
				printf("chdir / error!\n");
				exit(1);
			}
		}
	}
	end = syscall(228);
	printf("reading 50 files in 5 different directory costs %u ms\n", end - start);
}

int main(int argc, char const *argv[])
{
	init_env();
	
	test_write_single_file();

	test_write_large_file();

	test_multi_files();

	test_rw_multi_dir();

	return 0;
}
