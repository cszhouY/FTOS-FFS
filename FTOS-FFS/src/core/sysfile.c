//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <fcntl.h>
#include <string.h>

#include <aarch64/mmu.h>
#include <common/defines.h>
#include <common/spinlock.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/sleeplock.h>
#include <fs/file.h>
#include <fs/fs.h>

#include "syscall.h"

struct iovec {
    void *iov_base; /* Starting address. */
    usize iov_len;  /* Number of bytes to transfer. */
};

/*
 * Fetch the nth word-sized system call argument as a file descriptor
 * and return both the descriptor and the corresponding struct file.
 */
static int argfd(int n, i64 *pfd, struct file **pf) {
    i32 fd;
    struct file *f;

    if (argint(n, &fd) < 0)
        return -1;
    if (fd < 0 || fd >= NOFILE || (f = thiscpu()->proc->ofile[fd]) == 0)
        return -1;
    if (pfd)
        *pfd = fd;
    if (pf)
        *pf = f;
    return 0;
}

/*
 * Allocate a file descriptor for the given file.
 * Takes over file reference from caller on success.
 */
static int fdalloc(struct file *f) {
    /* TODO: Your code here. */
    for (int i = 0; i < NOFILE; i++) {
        if (thiscpu()->proc->ofile[i] == 0) {
            thiscpu()->proc->ofile[i] = f;
            return i;
        }
    }
    return -1;
}

int sys_dup() {
    /* TODO: Your code here. */
    struct file *f;
    if (argfd(0, 0, &f) < 0) {
        return -1;
    }

    int fd = fdalloc(f);
    if (fd < 0) {
        return -1;
    }
    filedup(f);
    return fd;
}

isize sys_read() {
    /* TODO: Your code here. */
    struct file *f;
    char *addr;
    i32 n;

    if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &addr, (usize)n) < 0) {
        return -1;
    }
    return fileread(f, addr, n);
}

isize sys_write() {
    /* TODO: Your code here. */
    struct file *f;
    char *addr;
    i32 n;

    if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &addr, (usize)n) < 0) {
        return -1;
    }
    return filewrite(f, addr, n);
}

isize sys_writev() {
    /* TODO: Your code here. */

    /* Example code.
     *
     * ```
     * struct file *f;
     * i64 fd, iovcnt;
     * struct iovec *iov, *p;
     * if (argfd(0, &fd, &f) < 0 ||
     *     argint(2, &iovcnt) < 0 ||
     *     argptr(1, &iov, iovcnt * sizeof(struct iovec)) < 0) {
     *     return -1;
     * }
     *
     * usize tot = 0;
     * for (p = iov; p < iov + iovcnt; p++) {
     *     // in_user(p, n) checks if va [p, p+n) lies in user address space.
     *     if (!in_user(p->iov_base, p->iov_len))
     *          return -1;
     *     tot += filewrite(f, p->iov_base, p->iov_len);
     * }
     * return tot;
     * ```
     */

    struct file *f;
    i32 fd, iovcnt;
    struct iovec *iov;
    if (argfd(0, (i64 *)&fd, &f) < 0 || argint(2, &iovcnt) < 0 ||
        argptr(1, (char **)&iov, (u64)iovcnt * sizeof(struct iovec)) < 0) {
        return -1;
    }
    usize tot = 0;
    for (struct iovec *p = iov; p < iov + iovcnt; p++) {
        if (0) {
            return -1;
        }
        tot += (usize)filewrite(f, p->iov_base, (isize)p->iov_len);
    }
    return (isize)tot;
}

int sys_close() {
    /* TODO: Your code here. */
    struct file *f;
    int fd;

    if (argfd(0, (i64 *)&fd, &f) < 0) {
        return -1;
    }

    thiscpu()->proc->ofile[fd] = 0;
    fileclose(f);

    return 0;
}

int sys_fstat() {
    /* TODO: Your code here. */
    struct file *f;
    struct stat *st;

    if (argfd(0, 0, &f) < 0 || argptr(1, (void *)&st, sizeof(*st)) < 0) {
        return -1;
    }

    return filestat(f, st);
}

int sys_fstatat() {
    i32 dirfd, flags;
    char *path;
    struct stat *st;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0 || argptr(2, (void *)&st, sizeof(*st)) < 0 ||
        argint(3, &flags) < 0)
        return -1;

    if (dirfd != AT_FDCWD) {
        printf("sys_fstatat: dirfd unimplemented\n");
        return -1;
    }
    if (flags != 0) {
        printf("sys_fstatat: flags unimplemented\n");
        return -1;
    }

    Inode *ip;
    OpContext ctx;
    bcache.begin_op(&ctx);
    if ((ip = namei(path, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.lock(ip);
    stati(ip, st);
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);

    return 0;
}

static int namecmp(const char *s, const char *t) {
    return strncmp(s, t, FILE_NAME_MAX_LENGTH);
}

int sys_unlink()
{
    Inode *ip, *dp;
    struct dirent de;
    char name[FILE_NAME_MAX_LENGTH], *path;
    usize off;

    // 读取系统调用传递的参数，参数只包含路径，否则直接退出
    if(argstr(0, &path) < 0) {
        printf("argstr(0, &path) < 0\n");
        return -1;
    }

    // 开始一次原子操作
    OpContext ctx;
    bcache.begin_op(&ctx);
    // 待删除对象不存在父目录，异常直接退出
    if((dp = nameiparent(path, name, &ctx)) == 0){
        bcache.end_op(&ctx);
        printf("(dp = nameiparent(path, name, &ctx)) == 0");
        return -1;
    }

    inodes.lock(dp);

    // 对于隐藏对象，无法进行删除操作
    if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
        goto bad;

    // 获取待删除对象以及其在父目录的目录项的偏移
    if((ip = inodes.get(inodes.lookup(dp, name, &off))) == 0)
        goto bad;

    inodes.lock(ip);

    if(ip->entry.num_links < 1)
        PANIC("unlink: nlink < 1");

    // printf("dp->entry.num_links: %d\n", dp->entry.num_links);
    
    // 无法删除非空文件夹
    if(ip->entry.type == INODE_DIRECTORY && !inodes.empty(ip)){
        inodes.unlock(ip);
        inodes.put(&ctx, ip);
        goto bad;
    }

    // 删除待删除对象在父目录中的项
    memset(&de, 0, sizeof(de));
    if(inodes.write(&ctx, dp, (u8*)&de, off * sizeof(de), sizeof(de)) != sizeof(de))
        PANIC("unlink: inode_write");
    // 待删除对象为目录类型，父目录需要额外减少num_links
    // 即删除对象内包含父目录的硬链接，删除对象后这一链接丢失
    if(ip->entry.type == INODE_DIRECTORY){
        dp->entry.num_links--;
        inodes.sync(&ctx, dp, 1);
    }
    inodes.unlock(dp);
    inodes.put(&ctx, dp);

    // 文件本身而言删除指向自身的硬链接
    ip->entry.num_links--;
    inodes.sync(&ctx, ip, 1);
    inodes.unlock(ip);
    inodes.put(&ctx, ip);

    bcache.end_op(&ctx);

    return 0;

// 无法删除的情况
bad:
    // printf ("bad!");
    inodes.unlock(dp);
    inodes.put(&ctx, dp);
    bcache.end_op(&ctx);
    return -1;
}    

Inode *create(char *path, short type, short major, short minor, OpContext *ctx) {
    /* TODO: Your code here. */
    u32 off;
    Inode *ip, *dp;
    char name[FILE_NAME_MAX_LENGTH] = {0};
    u32 gno;
    usize ino;

    // 目录是否存在
    if ((dp = nameiparent(path, name, ctx)) == 0) {
        return 0;
    }

    // 首先判断目标名称的文件是否存在于当前目录中
    inodes.lock(dp);
    // 此处要进行修改
    // 获取父目录的gno，对应type为普通文件的情况
    gno = ((u32)dp->inode_no - 1) / (NINODES / NGROUPS);

    // 文件名已存在
    if ((ino = inodes.lookup(dp, name, (usize *)&off)) != 0) {
        // printf("ino: %u\n", inodes.lookup(dp, name, (usize *)&off));
        ip = inodes.get(ino);
        inodes.unlock(dp);
        inodes.put(ctx, dp);
        inodes.lock(ip);
        if (type == INODE_REGULAR && ip->entry.type == INODE_REGULAR) {
            return ip;
        }
        inodes.unlock(ip);
        inodes.put(ctx, ip);
        return 0;
    }

    // 不存在，分配inode
    // 如果为目录类型，寻找最空闲的组并将组号赋值给gno
    // 如果为文件类型，从父目录所在块组开始按组分配inode，当前组满了考虑在下一组进行分配
    while((ino = inodes.allocg(ctx, (u16)type, gno)) == 0) {
        gno++;
    }

    // 无法按组分配inode，改为从头寻找空闲块进行分配
    if (gno >= NGROUPS && (ino = inodes.alloc(ctx, (u16)type)) == 0) {
        PANIC("create: inodes.alloc");
    }

    // 根据inode编号获取inode
    ip = inodes.get(ino);

    inodes.lock(ip);
    ip->entry.major = (u16)major;
    ip->entry.minor = (u16)minor;
    ip->entry.num_links = 1;
    inodes.sync(ctx, ip, 1);

    // 对于新建目录的情况，要额外考虑“.”与“..”两个文件
    if (type == INODE_DIRECTORY) {
        dp->entry.num_links++;
        inodes.sync(ctx, dp, 0);
        inodes.insert(ctx, ip, ".", ip->inode_no);
        inodes.insert(ctx, ip, "..", dp->inode_no);
    }
    inodes.insert(ctx, dp, name, ip->inode_no);

    inodes.unlock(dp);
    inodes.put(ctx, dp);

    return ip;
}

int sys_openat() {
    char *path;
    int dirfd, fd, omode;
    struct file *f;
    Inode *ip;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0 || argint(2, &omode) < 0)
        return -1;

    // printf("%d, %s, %lld\n", dirfd, path, omode);
    if (dirfd != AT_FDCWD) {
        printf("sys_openat: dirfd unimplemented\n");
        return -1;
    }
    // if ((omode & O_LARGEFILE) == 0) {
    //     printf("sys_openat: expect O_LARGEFILE in open flags\n");
    //     return -1;
    // }

    OpContext ctx;
    bcache.begin_op(&ctx);
    if (omode & O_CREAT) {
        // FIXME: Support acl mode.
        ip = create(path, INODE_REGULAR, 0, 0, &ctx);
        if (ip == 0) {
            bcache.end_op(&ctx);
            return -1;
        }
    } else {
        if ((ip = namei(path, &ctx)) == 0) {
            bcache.end_op(&ctx);
            return -1;
        }
        inodes.lock(ip);
        // if (ip->entry.type == INODE_DIRECTORY && omode != (O_RDONLY | O_LARGEFILE)) {
        //     inodes.unlock(ip);
        //     inodes.put(&ctx, ip);
        //     bcache.end_op(&ctx);
        //     return -1;
        // }
    }

    if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            fileclose(f);
        inodes.unlock(ip);
        inodes.put(&ctx, ip);
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    bcache.end_op(&ctx);

    f->type = FD_INODE;
    f->ip = ip;
    f->off = 0;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
    return fd;
}

int sys_mkdirat() {
    i32 dirfd, mode;
    char *path;
    Inode *ip;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0 || argint(2, &mode) < 0)
        return -1;
    if (dirfd != AT_FDCWD) {
        printf("sys_mkdirat: dirfd unimplemented\n");
        return -1;
    }
    if (mode != 0) {
        printf("sys_mkdirat: mode unimplemented\n");
        return -1;
    }
    OpContext ctx;
    bcache.begin_op(&ctx);
    if ((ip = create(path, INODE_DIRECTORY, 0, 0, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);
    return 0;
}

int sys_mknodat() {
    Inode *ip;
    char *path;
    i32 dirfd, major, minor;

    if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0 || argint(2, &major) < 0 || argint(3, &minor))
        return -1;

    if (dirfd != AT_FDCWD) {
        printf("sys_mknodat: dirfd unimplemented\n");
        return -1;
    }
    printf("mknodat: path '%s', major:minor %d:%d\n", path, major, minor);

    OpContext ctx;
    bcache.begin_op(&ctx);
    if ((ip = create(path, INODE_DEVICE, (i16)major, (i16)minor, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);
    return 0;
}

int sys_chdir() {
    char *path;
    Inode *ip;
    struct proc *curproc = thiscpu()->proc;

    OpContext ctx;
    bcache.begin_op(&ctx);
    if (argstr(0, &path) < 0 || (ip = namei(path, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.lock(ip);
    if (ip->entry.type != INODE_DIRECTORY) {
        inodes.unlock(ip);
        inodes.put(&ctx, ip);
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    inodes.put(&ctx, curproc->cwd);
    bcache.end_op(&ctx);
    curproc->cwd = ip;
    return 0;
}
int execve(const char *path, char *const argv[], char *const envp[]);
int sys_exec() {
    char *p;
    void *argv, *envp;
    if (argstr(0, &p) < 0 || argu64(1, (u64 *)&argv) < 0 || argu64(2, (u64 *)&envp) < 0)
        return -1;
    return execve(p, argv, envp);
}
