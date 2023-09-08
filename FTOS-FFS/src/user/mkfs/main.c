#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef uint8_t uchar;
typedef uint16_t ushort;
typedef uint32_t uint;

// this file should be compiled with normal gcc...

struct xv6_stat;
#define stat xv6_stat // avoid clash with host struct stat
#define sleep xv6_sleep
// #include "../../../inc/fs.h"
#include "../../fs/defines.h"
#include "../../fs/inode.h"

#ifndef static_assert
#define static_assert(a, b) \
    do                      \
    {                       \
        switch (0)          \
        case 0:             \
        case (a):;          \
    } while (0)
#endif

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

// FFS disk layout:
// [ MBR Block | super block | log blocks | block groups ]

// [ inode blocks | bitmap blocks | data blocks | ... | inode blocks | bitmap blocks | data blocks ]
// \---------------- block group ---------------/
#define BSIZE BLOCK_SIZE
#define LOGSIZE LOG_MAX_SIZE
#define NDIRECT INODE_NUM_DIRECT
#define NINDIRECT INODE_NUM_INDIRECT
#define DIRSIZ FILE_NAME_MAX_LENGTH

#define IPB (BSIZE / sizeof(InodeEntry))
#define BPG ((FSSIZE - 2 - LOGSIZE) / NGROUPS)
#define NIPG (NINODES / NGROUPS)
#define NINBLOCKS_PER_GROUP (NINDIRECT / (NGROUPS - 1 ) * 2 + 1)

#define IBLOCK(i, sb) (sb.bg_start + BPG * ((i - 1) / NIPG) + ((i - 1) % NIPG) / IPB)
#define IGROUP(i, sb) ((i - 1) / NIPG)
#define BGROUP(b, sb) ((b - 2 - LOG_MAX_SIZE) / BPG) 

int blocks_per_group = BPG;
int ninodeblocks_per_group = (NINODES / NGROUPS) / IPB + 1;
int nbitmap_per_group = BPG / (BIT_PER_BLOCK) + 1;

// int nbitmap = FSSIZE / (BSIZE * 8) + 1;
// int ninodeblocks = NINODES / IPB + 1;
int num_log_blocks = LOGSIZE;
int nmeta;           // Number of meta blocks (boot, sb, num_log_blocks, inode, bitmap)
int num_data_blocks; // Number of data blocks
/* 修改常量 */

int fsfd;
SuperBlock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;
uint used_block[NGROUPS] = {0};

// void balloc(int);
void wsect(uint, void *);
void winode(uint, struct dinode *);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
void wblock(uint bnum, void *buf);
void rblock(uint bnum, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);
void ballocg();

// convert to little-endian byte order
ushort xshort(ushort x)
{
    ushort y;
    uchar *a = (uchar *)&y;
    a[0] = x;
    a[1] = x >> 8;
    return y;
}

uint xint(uint x)
{
    uint y;
    uchar *a = (uchar *)&y;
    a[0] = x;
    a[1] = x >> 8;
    a[2] = x >> 16;
    a[3] = x >> 24;
    return y;
}

int main(int argc, char *argv[])
{
    int i, cc, fd;
    uint rootino, inum, off;
    struct dirent de;
    char buf[BSIZE];
    InodeEntry din;

    static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

    if (argc < 2)
    {
        fprintf(stderr, "Usage: mkfs fs.img files...\n");
        exit(1);
    }

    assert((BSIZE % sizeof(struct dinode)) == 0);
    assert((BSIZE % sizeof(struct dirent)) == 0);

    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fsfd < 0)
    {
        perror(argv[1]);
        exit(1);
    }

    /* 修改初始文件系统生成 */
    /* 修改超级块的初始化过程 */
    // 1 fs block = 1 disk sector
    nmeta = 2 + num_log_blocks + ninodeblocks_per_group * NGROUPS + nbitmap_per_group * NGROUPS;
    num_data_blocks = FSSIZE - nmeta;

    // sb.num_blocks = xint(FSSIZE);
    // sb.num_data_blocks = xint(num_data_blocks);
    // sb.num_inodes = xint(NINODES);
    // sb.num_log_blocks = xint(num_log_blocks);
    // sb.log_start = xint(2);
    // sb.inode_start = xint(2 + num_log_blocks);
    // sb.bitmap_start = xint(2 + num_log_blocks + ninodeblocks);

    sb.num_blocks = xint(FSSIZE);
    sb.num_log_blocks = xint(num_log_blocks);
    sb.num_groups = xint(NGROUPS);
    sb.num_inodes = xint(NINODES);
    sb.blocks_per_group = xint(blocks_per_group);

    sb.log_start = xint(2);
    sb.bg_start = xint(2 + num_log_blocks);

    sb.num_inodeblocks_per_group = xint(ninodeblocks_per_group);
    sb.num_bitmap_per_group = xint(nbitmap_per_group);
    sb.num_datablocks_per_group = xint(blocks_per_group - ninodeblocks_per_group - nbitmap_per_group);
    sb.bitmap_start_per_group = xint(ninodeblocks_per_group);
    sb.data_start_per_group = xint(ninodeblocks_per_group + nbitmap_per_group);

    // printf("nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks %u) blocks %d "
    //        "total %d\n",
    //        nmeta,
    //        num_log_blocks,
    //        ninodeblocks,
    //        nbitmap,
    //        num_data_blocks,
    //        FSSIZE);

    printf("nmeta %d (boot, super, log blocks %u, inode blocks per group %u, bitmap blocks per group %u) data blocks %d "
           "total %lu\n",
           nmeta,
           num_log_blocks,
           ninodeblocks_per_group,
           nbitmap_per_group,
           num_data_blocks,
           FSSIZE);

    freeblock = nmeta; // the first free block that we can allocate
    /* 修改超级块的初始化过程 */

    // 磁盘内容全部初始化
    for (i = 0; i < FSSIZE * SECTS_PER_BLOCK; i++)
        wsect(i, zeroes);

    // 将内容写入超级块
    memset(buf, 0, sizeof(buf));
    memmove(buf, &sb, sizeof(sb));
    wblock(1, buf);

    // 首先为根目录分配inode，同时保证此inode编号为1
    rootino = ialloc(INODE_DIRECTORY);
    assert(rootino == ROOT_INODE_NO);

    // 初始化当前目录，并将其与rootino关联
    bzero(&de, sizeof(de));
    de.inode_no = xshort(rootino);
    strcpy(de.name, ".");
    iappend(rootino, &de, sizeof(de));

    // 初始化父目录，并将其与rootino关联
    bzero(&de, sizeof(de));
    de.inode_no = xshort(rootino);
    strcpy(de.name, "..");
    iappend(rootino, &de, sizeof(de));

    // 初始化用户进程并将其写入磁盘中
    for (i = 2; i < argc; i++)
    {
        char *path = argv[i];
        int j = 0;
        for (; *argv[i]; argv[i]++)
        {
            if (*argv[i] == '/')
                j = -1;
            j++;
        }
        argv[i] -= j;
        printf("input: '%s' -> '%s'\n", path, argv[i]);

        assert(index(argv[i], '/') == 0);

        if ((fd = open(path, 0)) < 0)
        {
            perror(argv[i]);
            exit(1);
        }

        // Skip leading _ in name when writing to file system.
        // The binaries are named _rm, _cat, etc. to keep the
        // build operating system from trying to execute them
        // in place of system binaries like rm and cat.
        if (argv[i][0] == '_')
            ++argv[i];

        inum = ialloc(INODE_REGULAR);

        bzero(&de, sizeof(de));
        de.inode_no = xshort(inum);
        strncpy(de.name, argv[i], DIRSIZ);
        iappend(rootino, &de, sizeof(de));

        while ((cc = read(fd, buf, sizeof(buf))) > 0) {
            iappend(inum, buf, cc);            
        }

        rinode(xint(inum), &din);
       
        close(fd);
    }

    // fix size of root inode dir
    rinode(rootino, &din);
    off = xint(din.num_bytes);
    off = ((off / BSIZE) + 1) * BSIZE;
    din.num_bytes = xint(off);
    winode(rootino, &din);

    // 更新位图
    ballocg();

    exit(0);
}

// 将buf中的数据写入到指定扇区sec中
void wsect(uint sec, void *buf)
{
    if (lseek(fsfd, sec * SECT_SIZE, 0) != sec * SECT_SIZE)
    {
        perror("lseek");
        exit(1);
    }
    if (write(fsfd, buf, SECT_SIZE) != SECT_SIZE)
    {
        perror("write");
        exit(1);
    }
}

void rblock(uint bnum, void *buf)
{
    int sec;
    for (sec = 0; sec < SECTS_PER_BLOCK; sec++) {
        rsect(sec + bnum * SECTS_PER_BLOCK, buf + sec * SECT_SIZE);
    }
}

void wblock(uint bnum, void *buf)
{
    int sec;
    for (sec = 0; sec < SECTS_PER_BLOCK; sec++) {
        wsect(sec + bnum * SECTS_PER_BLOCK, buf + sec * SECT_SIZE);
    }
}

// 将ip对应的索引数据写入到编号为inum的inode
void winode(uint inum, struct dinode *ip)
{
    char buf[BSIZE];
    uint bn;
    struct dinode *dip;

    bn = IBLOCK(inum, sb);
    // printf("inum %d block %d\n", inum, bn);
    rblock(bn, buf);
    dip = ((struct dinode *)buf) + (inum % IPB);
    *dip = *ip;
    wblock(bn, buf);
}

// 将编号为inum的inode的索引数据读取到ip中
void rinode(uint inum, struct dinode *ip)
{
    char buf[BSIZE];
    uint bn;
    struct dinode *dip;

    bn = IBLOCK(inum, sb);
    rblock(bn, buf);
    dip = ((struct dinode *)buf) + (inum % IPB);
    *ip = *dip;
}

// 从指定扇区sec中读取数据到buf
void rsect(uint sec, void *buf)
{
    if (lseek(fsfd, sec * SECT_SIZE, 0) != sec * SECT_SIZE)
    {
        perror("lseek");
        exit(1);
    }
    if (read(fsfd, buf, SECT_SIZE) != SECT_SIZE)
    {
        perror("read");
        exit(1);
    }
}

// 分配指定type的inode结点
uint ialloc(ushort type)
{
    uint inum = freeinode++;
    struct dinode din;

    bzero(&din, sizeof(din));
    din.type = xshort(type);
    din.num_links = xshort(1);
    din.num_bytes = xint(0);
    winode(inum, &din);
    return inum;
}

// 根据已使用的磁盘块数used修改相应的bitmap
// void balloc(int used) {
//     uchar buf[BSIZE];
//     int i;

//     printf("balloc: first %d blocks have been allocated\n", used);
//     assert(used < BSIZE * 8);
//     bzero(buf, BSIZE);
//     for (i = 0; i < used; i++) {
//         buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
//     }
//     printf("balloc: write bitmap block at sector %d\n", sb.bitmap_start);
//     wsect(sb.bitmap_start, buf);
// }

// 直接根据used_block数组进行更新，因此移除了参数
void ballocg()
{
    uchar buf[BSIZE];
    int i;
    // 当前组内已使用数据块数
    int groupused;
    // 块组编号
    int gno;
    // 块组内偏移
    int data_start = sb.data_start_per_group;

    // 遍历used_block数组，依次更新相应块组内的位图
    for (gno = 0; gno < NGROUPS; gno++)
    {
        groupused = used_block[gno];
        printf("balloc: first %d datablocks in group %d have been allocated\n", groupused, gno);
        assert(groupused <= sb.num_datablocks_per_group);
        bzero(buf, BSIZE);
        // 位图更新逻辑
        for (i = 0; i < groupused + data_start; i++)
        {
            buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
        }
        printf("balloc: write bitmap block at sector %lu\n", sb.bg_start + sb.bitmap_start_per_group + gno * BPG);
        // 将更新后的位图数据写回位图块中
        wblock(sb.bg_start + sb.bitmap_start_per_group + gno * BPG, buf);
    }
}

#define min(a, b) ((a) < (b) ? (a) : (b))

// 将xp指向的大小为n的数据写入到inode为inum的data块中
// void iappend(uint inum, void *xp, int n)
// {
//     char *p = (char *)xp;
//     uint fbn, off, n1;
//     struct dinode din;
//     char buf[BSIZE];
//     uint indirect[NINDIRECT];
//     uint x;

//     rinode(inum, &din);
//     off = xint(din.num_bytes);
//     // printf("append inum %d at off %d sz %d\n", inum, off, n);
//     while (n > 0)
//     {
//         fbn = off / BSIZE;
//         assert(fbn < INODE_MAX_BLOCKS);
//         if (fbn < NDIRECT)
//         {
//             if (xint(din.addrs[fbn]) == 0)
//             {
//                 din.addrs[fbn] = xint(freeblock++);
//             }
//             x = xint(din.addrs[fbn]);
//         }
//         else
//         {
//             if (xint(din.indirect) == 0)
//             {
//                 din.indirect = xint(freeblock++);
//             }
//             rsect(xint(din.indirect), (char *)indirect);
//             if (indirect[fbn - NDIRECT] == 0)
//             {
//                 indirect[fbn - NDIRECT] = xint(freeblock++);
//                 wsect(xint(din.indirect), (char *)indirect);
//             }
//             x = xint(indirect[fbn - NDIRECT]);
//         }
//         n1 = min(n, (fbn + 1) * BSIZE - off);
//         rsect(x, buf);
//         bcopy(p, buf + off - (fbn * BSIZE), n1);
//         wsect(x, buf);
//         n -= n1;
//         off += n1;
//         p += n1;
//     }
//     din.num_bytes = xint(off);
//     winode(inum, &din);
// }
void iappend(uint inum, void *xp, int n)
{
    char *p = (char *)xp;
    // 当前inode内下一未分配数据块编号、数据偏移以及实际写入字节
    uint fbn, off, n1;
    // 临时磁盘inode结构
    struct dinode din;
    // 缓冲区
    char buf[BSIZE];
    // 间接块数组
    uint indirect[NINDIRECT];
    // 单次分配数据块的编号
    uint x;
    // 获取inode所在的组号
    uint gno = IGROUP(inum, sb);

    // 根据编号读取inode，获得文件偏移，即当前文件大小，
    rinode(inum, &din);
    off = xint(din.num_bytes);
    printf("append inum %d on group\n", inum);

    // 循环写入数据
    while (n > 0)
    {
        // 根据偏移获取当前inode内待分配的数据块编号
        fbn = off / BSIZE;
        assert(fbn < INODE_MAX_BLOCKS);

        // 获取默认的freeblock位置

        // 数据块优先考虑分配在当前块组中
        // 如果当前块组数据块已完全分配，则寻找下一个块组
        // 考虑到初始化操作，一般不会出现无法分配的问题
        while (used_block[gno] == sb.num_datablocks_per_group) 
            gno = (gno + 1) % NGROUPS;
        // 确定待分配组后确定组内待分配数据块，由used_block确定下一个可分配块
        freeblock = sb.bg_start + gno * BPG + sb.data_start_per_group + used_block[gno];

        // inode对应的直接块未完全分配
        if (fbn < NDIRECT)
        {
            // 直接分配到当前块组中
            if (xint(din.addrs[fbn]) == 0)
            {
                din.addrs[fbn] = xint(freeblock);
                used_block[gno]++;
                printf("append direct on group %d at off %d sz %d\n", gno, off, n);
            }
            x = xint(din.addrs[fbn]);
        }
        // 直接块已完全分配，考虑使用间接块
        else
        {
            // 间接索引块未分配，首先在当前块组中分配索引块
            if (xint(din.indirect) == 0)
            {
                din.indirect = xint(freeblock);
                used_block[gno]++;
                printf("append store block on group %d at off %d sz %d\n", gno, off, n);
            }
            rblock(xint(din.indirect), (char *)indirect);

            // 计算待分配的间接块编号
            uint indirect_no = fbn - NDIRECT;
            if (indirect[indirect_no] == 0)
            {
                // 这里考虑到大文件的分配逻辑，将间接块划分到不同的块组中
                // 从inode所在的块组开始，后续每一个块组分配一定数目的间接块
                // 根据间接块的编号获取分散的块所在的块组

                // 首先定位到后续第一个块组
                uint tgno =  (indirect_no / NINBLOCKS_PER_GROUP + 1) % NGROUPS;
                // 块组跨度默认为2
                uint step = 2;
                // 从后续第一个块组开始，以step为跨度寻找空闲块
                while (used_block[tgno] == sb.num_datablocks_per_group) {
                    tgno = tgno + step;
                    // 遍历到了块组末尾，这时考虑减小跨度从头开始重新分配
                    if (tgno >= NGROUPS) {
                        tgno = 0;
                        step = 1;
                    }
                    // 没有空闲块可用，直接退出
                    if (step == 1 && tgno >= NGROUPS) {
                        break;
                    }
                }

                // 确定了待分配块编号
                freeblock = sb.bg_start + tgno * BPG + sb.data_start_per_group + used_block[tgno];
                indirect[indirect_no] = xint(freeblock);
                used_block[tgno]++;
                printf("append indirect block on group %d at off %d sz %d\n", tgno, off, n);
                // 同时需要修改间接索引块
                wblock(xint(din.indirect), (char *)indirect);
            }
            x = xint(indirect[indirect_no]);
        }
        // 确定此次写入的数据大小
        n1 = min(n, (fbn + 1) * BSIZE - off);
        // 写回数据
        rblock(x, buf);
        bcopy(p, buf + off - (fbn * BSIZE), n1);
        wblock(x, buf);
        // 更新相应参数
        n -= n1;
        off += n1;
        p += n1;
    }
    // 更新磁盘inode中的文件大小属性
    din.num_bytes = xint(off);
    winode(inum, &din);
}    
