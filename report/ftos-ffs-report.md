# FTOS-FFS设计与实验报告

## 实验目标

* 实现块组磁盘结构
* 实现文件与目录的放置策略
* 实现大文件的优化放置策略
* shell程序编写

## 实验原理

### 快速文件系统（FFS）

快速文件系统（Fast File System，FFS）是一种为许多 UNIX 和 类UNIX 操作系统所使用的文件系统。它也被称为伯克利快速文件系统（Berkeley Fast File System），BSD 快速文件系统（BSD Fast File System），缩写为 `FFS`。`FFS` 在Unix文件系统之上，提供目录结构的信息，以及各种磁盘访问的优化。

`FFS` 的主要特点是重新设计了文件系统结构和分配策略，以“磁盘感知”的方式，从而提高性能。本实验的目的是在FTOS中实现一个简化的 `FFS`，核心设计思想如下：

* 将磁盘划分为**多个柱面组或块组**，每个组中包含文件系统的所有结构，如 `inode`、`数据块`、`位图` 等。
* 采用了一些简单的布局启发式方法处理放置逻辑，包括：

  * 同一目录下的文件优先放置在**父目录**所在的块组中
  * 新建目录优先放置在**使用率最低**的块组中
  * 大文件考虑**分块**后放置到**不同的块组**中
    > 本实验将使用间接数据块的文件视为大文件

    本实验的大文件分配方式如下：
    * 对于直接块与间接索引块，优先考虑在**父目录所在块组**中分配
    * 对于间接块，根据**块组大小**和**最大文件大小**预先计算出跨组分配允许的**单块组内最大间接块分配数目**，之后开始分配
      * 优先考虑分配到**下一块组**
      * 下一块组满则考虑**跨组分配**，
      * 如果上述分配方法无法全部分配，则从**首个块组**开始寻找空闲组**连续按组分配**，单块组分配数目与跨组分配一致

  * 对于其他不能按照上述分配的情况，进行**通常处理**
    > 本实验的处理方式为从头开始寻找首个空闲块，类似原始分配方式

### Shell

Shell是用户与类UNIX操作系统的交互界面，一般是通过命令行进行交互。

本实验的终端来源于xv6，其运行逻辑如下所示：

1. shell调用 **getcmd** 函数，从标准输入读取一行字符串，存储在buf数组中。
2. 然后，shell调用 **parseline** 函数，将buf中的字符串分割成多个token，存储在ps数组中。**token是以空格、换行、分号、管道符、重定向符等为分隔符的最小单元**。
   > 例如，命令 `ls -l | wc` 会被分割成 `ls`、`-l`、`|`、`wc` 四个token。
3. shell调用 **parse** 函数，根据token的类型和顺序，构造一个cmd结构体，表示解析后的命令。

    * **cmd结构体**是一个抽象的数据类型，它有多种子类型，分别对应不同的命令形式

        * `execcmd` 表示一个简单的可执行命令，如 `ls -l`
        * `redircmd` 表示一个带有重定向符的命令，如 `cat < file.txt`
        * `pipecmd` 表示一个带有管道符的命令，如 `ls -l | wc`
        * `listcmd` 表示一个带有分号的命令序列，如 `echo hello; echo world`
        * `backcmd` 表示一个带有后台运行符的命令，如 `sleep 10 &`

    * 每种子类型都有自己的成员变量，用于存储相关的信息。

        * `execcmd` 有 `argv` 和 `eargv` 两个数组，用于存储命令名和参数
        * `redircmd` 有 `cmd`、`file`、`fd`和`mode`四个变量，用于存储被重定向的命令、文件名、文件描述符和打开模式
        * `pipecmd` 有 `left` 和 `right` 两个变量，用于存储管道两端的命令
        * `listcmd` 有 `left` 和 `right` 两个变量，用于存储分号左右两边的命令
        * `backcmd` 有 `cmd` 一个变量，用于存储被后台运行的命令

4. shell调用 **runcmd** 函数，根据cmd结构体的类型和内容，执行相应的操作。
    * 如果cmd是 `execcmd` 类型，则调用**exec**函数执行可执行文件
    * 如果cmd是 `redircmd` 类型，则调用**open**函数打开文件，并将文件描述符复制到标准输入或输出上，并递归地执行被重定向的命令
    * 如果cmd是 `pipecmd` 类型，则调用**pipe**函数创建管道，并fork两个子进程分别执行管道两端的命令，并将管道连接到标准输入或输出上
    * 如果cmd是 `listcmd` 类型，则递归地执行分号左右两边的命令
    * 如果cmd是 `backcmd` 类型，则**fork**一个子进程执行被后台运行的命令，并不等待其结束

由于本实验shell已经完善，因此只需要编写用户程序即可，流程如下：

> 用户程序可以使用libc库函数，直接包含头文件即可

1. 在 `src/user` 里新建目录，以二进制的名称命名
2. 在该目录下新建 `main.c`，在里面编写这个二进制所有源代码，需要有 `int main(int argc, char *argv[])` 函数
3. 在 `src/user/CMakeLists.txt` 里的 `bin_list` 添加这个二进制的名字，加进编译列表
4. 在 `boot/CMakeLists.txt`里的 `user_files` 添加这个二进制的名字，加进编译列表（实验中没找到相应位置）

## 实验内容

### 实现块组磁盘结构

* 主要修改文件：`src/fs/defines.h`、`src/user/mkfs/main.c`、`src/fs/cache.c`、`src/fs/inode.c`
* 修改说明：

    1. 修改 `fs/defines.h` 中的超级块结构，新的超级块结构如下所示：

        ```c
        // FTOS磁盘结构:
        // [ MBR block | super block | log blocks | inode blocks | bitmap blocks | data blocks ]

        // 修改后的磁盘结构:
        // [ MBR Block | super block | log blocks | block groups ]

        // [ inode blocks | bitmap blocks | data blocks | ... ]
        // \---------------- block group ---------------/
        typedef struct {
            // 计数部分：添加块组数目、块组内块数目等信息
            u32 num_blocks;  // total number of blocks in filesystem.
            u32 num_log_blocks;  // number of blocks for logging, including log header.
            u32 num_groups; // number of blocks for grouping
            u32 num_inodes;
            u32 blocks_per_group; // number of blocks in a single block group

            // 块组外标记部分：删除位图、数据块起始相关信息，新增块组起始信息
            u32 log_start;       // the first block of logging area.
            u32 bg_start;    // the first block of block groups.

            // 块组内属性：
            // 新增块组内inode块数目/位图块数目/数据块数目
            // 新增块组内位图起始/数据块起始信息
            u32 num_inodeblocks_per_group; // number of inode blocks in a single block group
            u32 num_bitmap_per_group; // number of bitmap blocks in a single block group
            u32 num_datablocks_per_group; // number of data blocks in a single block group
            u32 bitmap_start_per_group; // the first block of bitmap blocks in a single block group
            u32 data_start_per_group; // the first block of data blocks in a single block group
        } SuperBlock;
        ```

    2. 修改 `fs/defines.h` 中相关宏定义，主要包括以下内容：

        * 新增 NIODES 宏以及 NGROUPS 宏定义inode数目与块组数目

            ```c
            // 后续主要供mkfs调用
            #define NINODES 200
            #define NGROUPS 10
            ```

        * 新增 SECT_SIZE 宏定义扇区大小，以便于mkfs中区分出块和扇区

            ```c
            #define SECT_SIZE 512
            ```

        * 修改 BLOCK_SIZE 宏，表示块大小

            ```c
            // 由于块大小改为4096会出现各种bug，暂时还是假定块大小等于扇区大小
            // 这里以及mkfs中的定义是为了将概念区分开来
            #define BLOCK_SIZE 512
            ```

    3. 修改 `mkfs/main.c` 中相关宏定义，主要包括以下内容：

        * 新增计数宏便于调用和初始化超级块，主要包括以下内容：

            ```c
            // 单个块内最大inode数目
            #define IPB (BSIZE / sizeof(InodeEntry))
            // 单个组内分配块数
            #define BPG ((FSSIZE - 2 - LOGSIZE) / NGROUPS)
            // 单个组内inode分配数目
            #define NIPG (NINODES / NGROUPS)
            // 单个组内最大间接数据块数目（用于大文件分组）
            #define NINBLOCKS_PER_GROUP (NINDIRECT / (NGROUPS - 1 ) * 2 + 1)
            ```

        * 新增定位宏便于根据inode或块编号定位到块或组，主要包括以下内容：

            ```c
            // 根据inode编号获取其所在的块编号
            #define IBLOCK(i, sb) (sb.bg_start + BPG * ((i - 1) / NIPG) + ((i - 1) % NIPG) / IPB)
            // 根据inode编号获取其所在的块组编号
            #define IGROUP(i, sb) ((i - 1) / NIPG)
            // 根据块编号获取其所在的块组编号
            #define BGROUP(b, sb) ((b - 2 - LOG_MAX_SIZE) / BPG)
            ```

    4. 修改 `mkfs/main.c` 相关变量定义，主要包括以下内容：

        * 新增块组内相关参数便于初始化超级块，如下所示：

            ```c
            // 块组内块分配数目
            int blocks_per_group = BPG;
            // 块组内inode块数目
            int ninodeblocks_per_group = (NINODES / NGROUPS) / IPB + 1;
            // 块组内位图块数目
            int nbitmap_per_group = BPG / (BIT_PER_BLOCK) + 1;
            ```

        * 新增长度为块组数目的数组，用于存储块组内部数据块使用数目，便于位图与用户程序写入磁盘时定位到空闲块

            ```c
            // 这一数组仅在mkfs中生效，文件系统中会重新定义类似变量以供相关函数调用
            // 默认初始化为0
            uint used_block[NGROUPS] = {0};
            ```

    5. 修改 `mkfs/main.c` 中相关函数声明以及定义，如下所示：

        * 新增rblock与wblock函数，将其与rsect和wsect区分开来，代码如下：

            ```c
            // 由于defines.h中将块和扇区区分开来，因此需要重新设计读写块的函数
            // 将编号为bnum的块的数据读取到buf中
            void rblock(uint bnum, void *buf)
            {
                int sec;
                // 循环读取块内的每一个扇区
                for (sec = 0; sec < SECTS_PER_BLOCK; sec++) {
                    rsect(sec + bnum * SECTS_PER_BLOCK, buf + sec * SECT_SIZE);
                }
            }
            // 将buf中的数据写入到编号为bnum的块
            void wblock(uint bnum, void *buf)
            {
                int sec;
                // 循环写入块内的每一个扇区
                for (sec = 0; sec < SECTS_PER_BLOCK; sec++) {
                    wsect(sec + bnum * SECTS_PER_BLOCK, buf + sec * SECT_SIZE);
                }
            }
            ```

        * 修改rsect与wsect函数，更换宏，代码如下：

            ```c
            // 将函数中的BSZIE宏更换为SECT_SIZE宏
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
            ```

        * 新增ballocg函数，替代原有的balloc函数进行位图更新操作，代码如下所示：

            ```c
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
            ```

        * 修改iappend函数，变更数据块选择逻辑，代码如下所示：

            ```c
            // iappend函数的主要功能是将xp位置的n字节数据
            // 写入到inode编号为inum的数据块中

            // 修改的目标有两个：
            // 将原始的数据块按序分配逻辑变为分组分配
            // 分组的选择最好满足大文件的优化放置策略
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
            ```

        * 修改主体函数结构，如下所示：

            ```c
            // 针对新的超级块结构，修改超级块初始化相关代码
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

            // 修改磁盘相关函数逻辑

            // 由于是以扇区为单位初始化磁盘，因此需要获取正确的扇区数目
            for (i = 0; i < FSSIZE * SECTS_PER_BLOCK; i++)
                wsect(i, zeroes);

            // 所有写入块的操作函数替换为wblock与rblock

            // 使用新的ballocg函数进行位图更新
            ballocg();
            ```

        > **至此，mkfs可以生成基于块组的磁盘结构，之后还需要调整inode、cache等相关函数以适应块组文件结构**

    6. 修改`fs/cache.c`中相关函数定义

        * 修改cache_alloc函数，以适应块组结构，代码如下所示：

            ```c
            // cache_alloc映射到alloc函数
            // 功能为从前往后遍历，找到第一个空闲的数据块并返回块编号
            static usize cache_alloc(OpContext *ctx) {
                // 修改后的alloc函数新增了块组编号这一层循环
                for (usize h = 0 ; h < sblock->num_groups; h++) {
                    for (usize i = 0; i < sblock->blocks_per_group; i += BIT_PER_BLOCK) {
                        // 由于新增了块组编号，块编号的计算方式也需要更新
                        // 任一块组位图块编号 = 块组起始 + 块组偏移块数 + 块组内位图起始 + 位图块偏移
                        usize block_no = sblock->bg_start +
                                        h * sblock->blocks_per_group +
                                        sblock->bitmap_start_per_group +
                                        i / BIT_PER_BLOCK;
                        Block *block = cache_acquire(block_no);

                        // 读取位图块信息后转化为BitmapCell数据结构
                        // 便于后续处理
                        BitmapCell *bitmap = (BitmapCell *)block->data;
                        // 遍历位图的每一位
                        for (usize j = 0; j < BIT_PER_BLOCK && i + j < sblock->blocks_per_group; j++) {
                            // 由于是顺序分配，因此找到第一个空闲位图则返回相应的块编号
                            if (!bitmap_get(bitmap, j)) {
                                bitmap_set(bitmap, j);
                                cache_sync(ctx, block);
                                cache_release(block);

                                block_no = sblock->bg_start + h * sblock->blocks_per_group + i + j;
                                block = cache_acquire(block_no);
                                memset(block->data, 0, BLOCK_SIZE);
                                cache_sync(ctx, block);
                                cache_release(block);

                                // 一旦新分配了一个数据块，更新块组的数据块使用信息
                                used_block[h]++;

                                return block_no;
                            }
                        }

                        cache_release(block);
                    }
                }
                PANIC("cache_alloc: no free block");
            }
            ```

    7. 修改`fs/inode.c`中相关宏定义，如下所示：

        ```c
        // 新增以下宏定义
        // 每个块组中的inode数目
        #define GINODES (sblock->num_inodes / sblock->num_groups)
        ```

    8. 修改`fs/inode.c`中相关函数定义，如下所示：

        * 修改通过inode编号获取块编号的内联函数，代码如下所示：

            ```c
            static INLINE usize to_block_no(usize inode_no) {
                // inode所在的块编号 =
                //      块组起始位置 + 块组偏移 + inode块组内偏移
                return sblock->bg_start +
                       sblock->blocks_per_group * ((inode_no - 1) / GINODES) +
                       (((inode_no - 1) % GINODES) / (INODE_PER_BLOCK));
            }
            ```

        * 修改inode_alloc函数的返回值，与后续新增函数统一
  
            ```c
            static usize inode_alloc(OpContext *ctx, InodeType type) {
                assert(type != INODE_INVALID);
                for (usize ino = 1; ino <= sblock -> num_inodes; ino++) {
                    usize block_no = to_block_no(ino);
                    // printf("inode %u in block %u\n", ino, block_no);
                    Block *block = cache->acquire(block_no);
                    InodeEntry *inode = get_entry(block, ino);

                    if (inode->type == INODE_INVALID) {
                        memset(inode, 0, sizeof(InodeEntry));
                        inode->type = type;
                        cache->sync(ctx, block);
                        cache->release(block);
                        return ino;
                    }

                    cache->release(block);
                }
                // 将PANIC替换为返回0
                return 0;
            }            
            ```

        > 至此，基于块组的磁盘结构基本改写完成，系统可以正常运行

* 总结

    除了上述部分，还处理了inode的边界问题，临界inode修改后也可以使用。

    块组磁盘结构的修改可以分为两部分：**初始磁盘结构生成**与**文件系统结构支持**，其中mkfs部分的修改侧重于前者，而inode、cache等相关文件则是侧重于后者。

### 实现文件/目录放置策略

* 主要修改文件：`src/fs/cache.c`、`src/fs/inode.c`、`src/core/sysfile.c`、`src/fs/fs/c`
* 新增文件：`fs/used_block.h`
* 修改说明：

    1. 新增`fs/used_block.h`，代码如下所示：

        ```c
        #ifndef USED_BLOCK_H
        #define USED_BLOCK_H

        #define u32 unsigned int

        // used_block功能类似mkfs中的同名变量
        // 主要用于统计各块组内已使用数据块数目
        // 考虑到多文件链接操作，实际的used_block定义位于cache.c中
        extern u32 used_block[NGROUPS];

        #endif
        ```

    2. 修改`fs/fs.c`中相关数据定义，如下所示：

        ```c
        // 用于临时存储位图
        static u8 used_block_data[BLOCK_SIZE];
        // 共享used_block数组，fs中主要是根据位图初始化used_block
        extern u32 used_block[NGROUPS];
        ```

    3. 修改`fs/fs.c`中主体函数，如下所示：

        ```c
        void init_filesystem() {
            init_block_device();
            // printf("init_block_device finished.\n");
            const SuperBlock *sblock = get_super_block();
            init_bcache(sblock, &block_device);

            // 初始化块设备与cache完成
            // 根据超级块内信息读取相应bitmap并更新used_block
            u32 i, j, h;
            for (h = 0; h < NGROUPS; h++) {
                for (i = 0; i < sblock->blocks_per_group; i += BIT_PER_BLOCK) {
                    // 调用块设备的read函数读取位图信息
                    block_device.read(sblock->bg_start +
                                    h * sblock->blocks_per_group +
                                    sblock->bitmap_start_per_group +
                                    i / BIT_PER_BLOCK, used_block_data);
                    // 转为BitmapCell结构
                    BitmapCell *bitmap = (BitmapCell *)used_block_data;
                    for (j = sblock->data_start_per_group; i + j < BIT_PER_BLOCK && j < sblock->blocks_per_group; j++) {
                        // 遍历位图，更新数据块使用数目
                        if (bitmap_get(bitmap, j))  {
                            used_block[h]++;
                        }
                    }
                }
                // printf("used_block: %u\n", used_block[h]);
            }
            // printf("init_bcache finished.\n");
            init_inodes(sblock, &bcache);
            // printf("init_inodes finished.\n");
        }
        ```

    4. 修改`fs/cache.h`中**BlockCache**结构，代码如下所示：

        ```c
        // 新增allocg函数指针定义，功能为在特定组中分配数据块
        usize (*allocg)(OpContext *ctx, u32 gno);
        ```

    5. 修改`fs/cache.c`中相关变量定义，代码如下所示：

        ```c
        // 新增used_block数据，主要作用域为fs.c、cache.c以及inode.c
        // 在cache.c中进行定义，通过used_block.h进行共享
        // cache.c中主要用于在文件系统运行实时更新块组使用情况
        u32 used_block[NGROUPS] = {0};
        ```

    6. 修改`fs/cache.c`中相关函数定义

        * 新增cache_allocg函数，在特定组内分配数据块，代码如下所示：

            ```c
            // 用于在特定块组内分配数据块
            static usize cache_allocg(OpContext *ctx, u32 gno) {
                // 限定合法块组编号
                assert(gno < NGROUPS);

                // 由于给定了块组编号，不需要额外的块组循环
                // 其余逻辑与cache_alloc一致
                for (usize i = 0; i < sblock->blocks_per_group; i += BIT_PER_BLOCK) {
                    usize block_no = sblock->bg_start +
                                    sblock->bitmap_start_per_group + gno * sblock->blocks_per_group +
                                    i / BIT_PER_BLOCK;
                    Block *block = cache_acquire(block_no);

                    BitmapCell *bitmap = (BitmapCell *)block->data;
                    for (usize j = 0; j < BIT_PER_BLOCK && i + j < sblock->blocks_per_group; j++) {
                        if (!bitmap_get(bitmap, j)) {
                            bitmap_set(bitmap, j);
                            cache_sync(ctx, block);
                            cache_release(block);

                            block_no = sblock->bg_start + gno * sblock->blocks_per_group + i + j;
                            block = cache_acquire(block_no);
                            memset(block->data, 0, BLOCK_SIZE);
                            cache_sync(ctx, block);
                            cache_release(block);

                            used_block[gno]++;

                            return block_no;
                        }
                    }

                    cache_release(block);
                }
                return 0;
            }
            ```

    7. 修改`fs/cache.c`中初始化操作，代码如下所示：

        ```c
        // 初始化bcache结构
        BlockCache bcache = {
            .get_num_cached_blocks = get_num_cached_blocks,
            .acquire = cache_acquire,
            .release = cache_release,
            .begin_op = cache_begin_op,
            .sync = cache_sync,
            .end_op = cache_end_op,
            .alloc = cache_alloc,
            // 新增allocg的初始化
            .allocg = cache_allocg,
            .free = cache_free,
        };
        ```

    8. 修改`fs/inode.h`中**InodeTree**结构，代码如下所示：

        ```c
        // 新增allocg函数指针，用于在特定块组中分配指定类型的inode
        usize (*allocg)(OpContext *ctx, InodeType type, u32 gno);
        ```

    9. 修改`fs/inode.c`中相关变量定义，如下所示：

        ```c
        // 与cache.c和fs.c共享used_block变量
        // inode.c中主要功能是供inode_alloc_group判断块组空闲情况
        extern u32 used_block[NGROUPS];
        ```

    10. 修改`fs/inode.c`中相关函数定义，如下所示：

        * 新增inode_alloc_group函数，用于在特定块组内分配inode，代码如下所示：

            ```c
            static usize inode_alloc_group(OpContext *ctx, InodeType type, u32 gno) {
                assert(type != INODE_INVALID);

                // 针对type为目录的情况，自动调整gno为最空闲的块组编号
                // 否则直接在编号为gno的块组中分配inode
                if (type == INODE_DIRECTORY) {
                    u32 i, used = FSSIZE;
                    // 遍历所有块组，通过used_block信息获取最空闲的块组编号
                    for (i = 0; i < NGROUPS; i++) {
                        if (used_block[i] < used) {
                            used = used_block[i];
                            gno = i;
                        }
                    }
                    if (i == NGROUPS)
                        return 0;
                }

                // printf("inode_allog gno: %u\n", gno);

                // ino的含义变为某一块组内相对的inode编号
                // ino在块组内顺序分配
                for (; gno < NGROUPS; gno++) {
                    for (usize ino = 1; ino <= GINODES; ino++) {
                        // tino为换算后的实际inode编号
                        usize tino = ino + gno * (NINODES / NGROUPS);

                        assert(tino <= NINODES);

                        // 获取待分配的inode所在的块编号
                        usize block_no = to_block_no(tino);
                        // printf("inode %u in block %u\n", ino, block_no);
                        Block *block = cache->acquire(block_no);
                        InodeEntry *inode = get_entry(block, tino);

                        // 找到空闲inode，进行分配并返回此inode编号
                        if (inode->type == INODE_INVALID) {
                            memset(inode, 0, sizeof(InodeEntry));
                            inode->type = type;
                            cache->sync(ctx, block);
                            cache->release(block); 
                            return tino;
                        }

                        cache->release(block);
                    }
                }
                // 这里表明分配失败
                return 0;
            }  
            ```

    11. 修改`fs/inode.c`中初始化操作，代码如下所示：

        ```c
        InodeTree inodes = {
            .alloc = inode_alloc,
            // 新增allocg的初始化操作，初始化为新增函数inode_alloc_group
            .allocg = inode_alloc_group,
            .lock = inode_lock,
            .unlock = inode_unlock,
            .sync = inode_sync,
            .get = inode_get,
            .clear = inode_clear,
            .share = inode_share,
            .put = inode_put,
            .read = inode_read,
            .write = inode_write,
            .lookup = inode_lookup,
            .insert = inode_insert,
            .remove = inode_remove,
        };
        ```

    12. 修改`core/sysfile.c`中函数定义，代码如下所示：

        * 修改create函数，以支持分配inode时区分文件和目录

            ```c
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
                if ((ino = inodes.allocg(ctx, (u16)type, gno)) == 0) {
                    // 无空闲块
                    if ((ino = inodes.alloc(ctx, (u16)type)) == 0) {
                        PANIC("create: inodes.alloc");
                    }
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
            ```

            > 至此小文件与目录的放置策略完成

* 总结

    此环节中主要实现两个策略：新建文件优先分配到当前块组，新建目录优先分配到最空闲块组。前者通过获取父目录的块组编号确定分配位置，后者通过遍历used_block数组获取最空闲块组，最后通过新定义的**inode_alloc_group**与**cache_allocg**进行指定块组的inode和data块分配。

### 实现大文件优化放置策略

* 主要修改文件：`/src/fs/inode.c`
* 修改说明：

    1. 再次修改`fs/inode.c`中相关宏定义，如下所示：

        ```c
        // 新增以下宏定义
        // 每个inode可在单个块组中分配的间接块数目，主要用于大文件处理
        // 本实验中大文件的定义为超出了直接块数目
        // 这里的分块数目算法核心是间隔分组，因此会有一个乘2的操作
        #define NINBLOCKS_PER_GROUP ((BLOCK_SIZE / sizeof(u32)) / (NGROUPS - 1) * 2 + 1)
        ```

    2. 修改`fs/inode.c`中相关函数定义，如下所示：

        * 修改inode_map函数，对于大文件的数据块分配进行特殊处理，代码如下所示：

            ```c
            static usize inode_map(OpContext *ctx, Inode *inode, usize offset, bool *modified) {
                InodeEntry *entry = &inode->entry;
                usize index = offset / BLOCK_SIZE;

                // 获取当前inode所在块组编号
                u32 gno = ((u32)inode->inode_no - 1) / (NINODES / NGROUPS);

                // 小文件，可以完全放置在当前块组中，否则按序查找其他块组
                if (index < INODE_NUM_DIRECT) {
                    if (entry->addrs[index] == 0) {
                        // 从父目录块组开始顺序寻找可分配块组
                        if ((entry->addrs[index] = (u32)cache->allocg(ctx, gno)) == 0) {
                            // 否则使用默认alloc函数进行分配
                            entry->addrs[index] = (u32)cache->alloc(ctx);
                        }
                        set_flag(modified);
                    }

                    return entry->addrs[index];
                }

                index -= INODE_NUM_DIRECT;
                assert(index < INODE_NUM_INDIRECT);

                // 分配间接块索引块，与小文件处理方式一致
                if (entry->indirect == 0) {
                    if ((entry->indirect = (u32)cache->allocg(ctx, gno)) == 0) {
                        // 否则使用默认alloc函数进行分配
                        entry->indirect = (u32)cache->alloc(ctx);
                    }
                    set_flag(modified);
                }

                Block *block = cache->acquire(entry->indirect);
                u32 *addrs = get_addrs(block);

                // 分配间接块，分配逻辑与mkfs中初始用户程序的分配方式类似
                if (addrs[index] == 0) {
                    // 根据间接块编号获取目标块组，默认为下一块组
                    usize tgno =  (index / NINBLOCKS_PER_GROUP + 1) % NGROUPS;
                    usize step = 2;
                    // 优先间隔分配
                    while (used_block[tgno] == sblock->num_datablocks_per_group) {
                        tgno = tgno + step;
                        // 如果间隔分配到达块组尾，则从头开始按相邻块组进行分配
                        if (tgno >= NGROUPS) {
                            tgno = 0;
                            step = 1;
                        }
                        // 无法分配块组，直接跳出，交给allocg函数处理异常
                        if (step == 1 && tgno >= NGROUPS) {
                            break;
                        }
                    }

                    addrs[index] = (u32)cache->allocg(ctx, (u32)tgno);
                    cache->sync(ctx, block);
                    set_flag(modified);
                }

                usize addr = addrs[index];
                cache->release(block);
                return addr;
            }   
            ```

            > 至此完成了对大文件的支持

* 总结

    大文件的处理方式是将数据块分散到各个分组中存储，同时确保不过于分散以保证一定程度的读写性能。**inode_map**函数为特定的inode分配数据块，对打文件的支持只需修改内部增加分支情况。

### shell程序编写

#### echo

* 基本功能及用法

    `echo` 在shell中用于打印文本内容，用法如下：

    ```shell
    eight@eight:~$ echo a
    eight@eight:~$ a
    ```

* 代码说明

    `echo` 的代码较为简单，如下所示：

    ```c
    int main(int argc, char *argv[])
    {
        int i;

        for (i = 1; i < argc; i++)
            // 对于echo中带有空格的部分，判断是否为文本尾
            // 由此决定打印空格还是换行符
            printf("%s%s", argv[i], i + 1 < argc ? " " : "\n");

        return 0;
    }
    ```

#### cat

* 基本功能及用法

    `cat` 在shell中用于显示文本文件的内容，用法如下：

    > 假设a.txt的内容尾abc

    ```shell
    eight@eight:~$ cat a.txt
    eight@eight:~$ abc
    ```

* 代码说明

    `cat` 的运行逻辑为使用open库函数读取文件，如下所示：

    ```c
    // 缓存区，用于临时存储文本
    char buf[512];

    void cat(int fd)
    {
        int n;

        // 循环读取fd中的内容直到文本尾
        // 每次读取都将缓存内容写入（打印）到控制台中
        while ((n = read(fd, buf, sizeof(buf))) > 0)
            write(1, buf, n); // 1为标准输出流
        // 读取失败
        if (n < 0)
        {
            printf("cat: read error\n");
            return;
        }
    }

    int main(int argc, char *argv[])
    {
        int fd, i;

        if (argc <= 1)
        {
            printf("Usage: cat files...\n");
            return 0;
        }

        // 对于多个对象，cat依次打印文本内容
        for (i = 1; i < argc; i++)
        {
            // FTOS限制，打开标签必须为0000
            if ((fd = open(argv[i], 0000)) < 0)
            {
                printf("cat: cannot open %s\n", argv[i]);
                return 0;
            }
            cat(fd);
            close(fd);
        }
        return 0;
    }
    ```

#### mkdir

* 基本功能

    `mkdir` 在shell中用于新建目录，用法如下：

    > 假设当前目录存在一个子目录a，要在a中新建一个目录b

    ```shell
    eight@eight:~$ mkdir ./a/b
    eight@eight:~$ cd a && ls
    b
    eight@eight:~/a$
    ```

* 代码说明

    `mkdir` 主要依靠 **mkdir** 调用创建目录，如下所示：

    ```c
    int main(int argc, char *argv[])
    {
        int i;

        if (argc < 2)
        {
            printf("Usage: mkdir files...\n");
            return 0;
        }

        // 对于多个目录，mkdir依次创建
        for (i = 1; i < argc; i++)
        {
            // 同样由于FTOS限制，flags必须为0000
            if (mkdir(argv[i], 0000) < 0)
            {
                printf("mkdir: %s failed to create\n", argv[i]);
                break;
            }
        }

        return 0;
    }
    ```

#### ls

* 基本功能

    `ls` 在shell中主要用于展示当前目录下的所有文件以及子目录，用法如下：

    > 假设当前目录有一个子目录test，test内有a、b和c三个文件

    ```shell
    eight@eight:~$ls
    test
    eight@eight:~$ls ./test
    a   b   c
    ```

* 代码说明

    `ls` 的核心逻辑在于遍历当前目录下的所有文件以及子目录，针对FTOS最好能够打印处隐藏文件（.与..）以及基本文件信息（比如文件分配的inode号）等，代码如下：

    ```c
    // 此函数的功能是根据完整路径获取最后一个‘/’之后的文件名
    char *fmtname(char *path)
    {
        static char buf[FILE_NAME_MAX_LENGTH + 1];
        char *p;

        // 完整路径从后向前遍历，直到遇到第一个‘/’
        for (p = path + strlen(path); p >= path && *p != '/'; p--)
            ;
        p++;

        // 文件名足够长，不需要填充空格
        if (strlen(p) >= FILE_NAME_MAX_LENGTH)
            return p;

        // 否则使用空格填充文件名并返回
        memmove(buf, p, strlen(p));
        memset(buf + strlen(p), ' ', FILE_NAME_MAX_LENGTH - strlen(p));
        return buf;
    }

    // 核心程序，遍历path下的所有文件以及子目录并打印
    void ls(char *path)
    {
        char buf[512], *p;
        // 文件描述符
        int fd;
        // 目录项，用于遍历目录下文件及子目录
        struct dirent de;
        // 用于存储文件状态，便于打印
        struct stat st;

        // 文件打开失败
        if ((fd = open(path, 0)) < 0)
        {
            printf("ls: cannot open %s\n", path);
            return;
        }
        // 获取文件状态信息失败
        if (fstat(fd, &st) < 0)
        {
            printf("ls: cannot stat %s\n", path);
            close(fd);
            return;
        }

        // 根据文件状态信息中的“st_mode”字段判断当前path对应的文件类型
        switch (st.st_mode)
        {
        // 通常文件，直接打印相关信息即可
        case S_IFREG:
            printf("%s %d %d %d\n", fmtname(path), st.st_mode, st.st_ino, st.st_size);
            break;
        // 目录文件，需要打印目录下的所有文件和子目录
        case S_IFDIR:
            // 完整路径过长，buf无法存储
            if (strlen(path) + 1 + FILE_NAME_MAX_LENGTH + 1 > sizeof buf)
            {
                printf("ls: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);
            // 手动添加最后一个‘/’，后续在‘/’后接上目录下的文件名
            *p++ = '/';
            // 循环将目录信息读取到de的inode_no以及name中
            while (read(fd, &de, sizeof(de)) == sizeof(de))
            {
                // 表明目录为空
                if (de.inode_no == 0)
                    continue;
                memmove(p, de.name, FILE_NAME_MAX_LENGTH);
                p[FILE_NAME_MAX_LENGTH] = 0;
                // 无法获取文件信息
                if (stat(buf, &st) < 0)
                {
                    printf("ls: cannot stat %s\n", buf);
                    continue;
                }
                // 打印相关信息
                printf("%s %d %d %d\n", fmtname(buf), st.st_mode, st.st_ino, st.st_size);
            }
            break;
        }
        close(fd);
    }

    // 主程序
    int main(int argc, char *argv[])
    {
        int i;

        // ls如果没有指定路径则默认为当前路径
        if (argc < 2)
        {
            ls(".");
            return 0;
        }
        // 将待遍历路径循环传入ls中并调用
        for (i = 1; i < argc; i++)
            ls(argv[i]);

        return 0;
    }
    ```

#### rm

* 基本功能

    `rm` 在shell中用于删除指定文件或目录，用法如下

    > 假设当前目录存在一个子目录a，a中不包含任何文件或目录

    ```shell
    eight@eight:~$ ls
    a
    eight@eight:~$ rm a
    eight@eight:~$ ls
    eight@eight:~$ 
    ```

* 代码说明

    `rm` 主要依靠 **myunlink** 函数进行删除操作，如下所示：

    ```c
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
    ```

    重点说明一下87号系统调用，位于 `src/core/sysfile.c`，代码如下所示：

    ```c
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
    ```

    系统调用内的 **inodes.empty** 函数用于判断某个inode对应的目录是否为空（不包括.和..），相应地在`src/fs/inode.c`和`src/fs/inode.h`中进行了修改，代码如下：

    ```c
    static usize inode_empty(Inode *inode) {
        InodeEntry *entry = &inode->entry;
        // printf("inode %u, name %s, inode_type %u\n", inode->inode_no, name, inode->entry.type);
        assert(entry->type == INODE_DIRECTORY);

        DirEntry dentry;
        // 跳过.与..进行遍历
        for (usize offset = 2 * sizeof(dentry); offset < entry->num_bytes; offset += sizeof(dentry)) {
            if (inode_read(inode, (u8 *)&dentry, offset, sizeof(dentry)) != sizeof(dentry))
                PANIC("inode_empty");
            // 某一目录项的inode编号不为0，说明存在文件
            if (dentry.inode_no != 0)
                return 0;
        }
        return 1;
    }    
    ```

    之后，在同目录下的 `syscall.h` 中，修改系统调用表（`syscall_table` 与 `syscall_table_str`），如下所示：

    ```c
    // syscall_table_str 也是类似的格式，不进行额外展示
    int (*syscall_table[NR_SYSCALL])() = {[0 ... NR_SYSCALL - 1] = sys_default,
                                        [SYS_set_tid_address] = sys_gettid,
                                        [SYS_ioctl] = sys_ioctl,
                                        [SYS_gettid] = sys_gettid,
                                        [SYS_rt_sigprocmask] = sys_sigprocmask,
                                        [SYS_brk] = (int (*)())sys_brk,
                                        [SYS_execve] = sys_exec,
                                        [SYS_sched_yield] = sys_yield,
                                        [SYS_clone] = sys_clone,
                                        [SYS_wait4] = sys_wait4,
                                        [SYS_exit_group] = sys_exit,
                                        [SYS_exit] = sys_exit,
                                        [SYS_dup] = sys_dup,
                                        [SYS_chdir] = sys_chdir,
                                        [SYS_fstat] = sys_fstat,
                                        [SYS_newfstatat] = sys_fstatat,
                                        [SYS_mkdirat] = sys_mkdirat,
                                        [SYS_mknodat] = sys_mknodat,
                                        [87] = sys_unlink,  // 新增87号系统调用
                                        [SYS_openat] = sys_openat,
                                        [SYS_writev] = (int (*)())sys_writev,
                                        [SYS_read] = (int (*)())sys_read,
                                        [SYS_write] = (int (*)())sys_write,
                                        [SYS_close] = sys_close,
                                        [SYS_myyield] = sys_yield};    
    ```

## 实验验证

### FFS功能验证

* 块组结构

    `mkfs` 创建初始文件镜像后将用户程序写入镜像中，最后更新bitmap。下面是 `mkfs` 中位图更新函数的运行结果：

    ```text
    balloc: first 92 datablocks in group 0 have been allocated
    balloc: write bitmap block at sector 68
    balloc: first 96 datablocks in group 1 have been allocated
    balloc: write bitmap block at sector 168
    balloc: first 96 datablocks in group 2 have been allocated
    balloc: write bitmap block at sector 268
    balloc: first 96 datablocks in group 3 have been allocated
    balloc: write bitmap block at sector 368
    balloc: first 32 datablocks in group 4 have been allocated
    balloc: write bitmap block at sector 468
    balloc: first 3 datablocks in group 5 have been allocated
    balloc: write bitmap block at sector 568
    balloc: first 0 datablocks in group 6 have been allocated
    balloc: write bitmap block at sector 668
    balloc: first 0 datablocks in group 7 have been allocated
    balloc: write bitmap block at sector 768
    balloc: first 0 datablocks in group 8 have been allocated
    balloc: write bitmap block at sector 868
    balloc: first 0 datablocks in group 9 have been allocated
    balloc: write bitmap block at sector 968
    ```

    由于用户程序大部分都被划分为大文件，因此文件数据块被分组放置。

* 文件/目录分配逻辑

    1. 进入文件系统后，使用 `ls` 命令查看当前根目录文件以及子目录：

        ```shell
        $ ls
        .              16384 1 512
        ..             16384 1 512
        init           32768 2 13680
        sh             32768 3 38720
        echo           32768 4 30024
        cat            32768 5 30024
        mkdir          32768 6 30024
        ls             32768 7 34120
        test           32768 8 30264
        console        0 9 0
        $ 
        ```

        > 文件名后从左至右依次为：**文件类型** **文件的inode编号** **文件大小**

        根目录下的文件inode都集中在根目录所在的块组中。

    2. 之后在根目录使用 `echo` 重定向输出来新建文件，并再次使用 `ls` 查看文件情况：

        ```shell
        $ echo a > a.txt
        $ ls
        .              16384 1 512
        ..             16384 1 512
        init           32768 2 13680
        sh             32768 3 38720
        echo           32768 4 30024
        cat            32768 5 30024
        mkdir          32768 6 30024
        ls             32768 7 34120
        test           32768 8 30264
        console        0 9 0
        a.txt          32768 10 2
        $ 
        ```

        新文件的inode仍然是在根目录所在的块组中。

    3. 使用 `mkdir` 新建目录

        ```shell
        $ mkdir my
        $ ls
        .              16384 1 512
        ..             16384 1 512
        init           32768 2 13680
        sh             32768 3 38720
        echo           32768 4 30024
        cat            32768 5 30024
        mkdir          32768 6 30024
        ls             32768 7 34120
        test           32768 8 30264
        console        0 9 0
        a.txt          32768 10 2
        my             16384 121 32
        $ 
        ```

        > 单个块组可包含的最大inode数目固定，这里测试的最大inode数目为20

        新建的目录被分配到了第七个块组中（编号为6），而这个块组是使用率最低的块组，使用率如下：

        ```text
        used_block: 93
        used_block: 96
        used_block: 96
        used_block: 96
        used_block: 32
        used_block: 3
        used_block: 0 // 首个最空闲块组
        used_block: 0
        used_block: 0
        used_block: 0
        ```

        因此目录被分配到了这一块组中，也就是将目录分配到最空闲的块组中。

    4. 在新建的目录中使用 `echo` 和重定向创建新文件：

        ```shell
        $ cd my
        $ /ls
        .              16384 121 512
        ..             16384 1 512
        $ /echo a > a.txt
        $ /ls
        .              16384 121 48
        ..             16384 1 512
        a.txt          32768 122 2
        ```

        > 由于命令以用户程序形式存储在根目录中，因此当前工作目录不是根目录时需要手动定位命令位置，即需要加上"/"，这一点可以通过设计环境变量解决

        **my**目录下的新文件被分配到了**my**所在的块组中。

* 大文件分配逻辑

    1. 使用自定义的用户程序在文件系统中创建一个大文件：

        ```shell
        $ test
        write 140 x 512 bytes, cost 677 ms
        $ ls
        .              16384 1 512
        ..             16384 1 512
        init           32768 2 13680
        sh             32768 3 38720
        echo           32768 4 30024
        cat            32768 5 30024
        mkdir          32768 6 30024
        ls             32768 7 34120
        test           32768 8 30264
        console        0 9 0
        testbuf        32768 10 71680     
        $    
        ```

        > 此处test创建一个极限大小（测试时的单文件最大大小为140个扇区）的全“0”文件，并且写入到磁盘中

    2. 对比前后数据块的分配情况，主要参考used_block数组：

        ```text
        used_block 0: 92 -> 96
        used_block 1: 96 -> 96
        used_block 2: 96 -> 96
        used_block 3: 96 -> 96
        used_block 4: 32 -> 96
        used_block 5: 3  -> 73
        used_block 6: 0  -> 3
        used_block 7: 0
        used_block 8: 0
        used_block 9: 0
        ```

    前后分组情况满足***实验原理***环节中的大文件分块方法，具体分块方式如下：

    1. 分配直接块与间接索引块
       * 父目录所在**块组（0）分块：直接块 4**（测试的直接块数目为12，间接块数目为1）
       * 父目录所在块组满，寻找**空闲块组（4）分块：直接块 8 + 间接索引块 1**
    2. 分配间接块，以父目录所在块组（0）的下一块组（1）为起始块组进行分配：
       * 目标块组（1），块组（1）满
       * 开始跨组分配，目标块组（3），块组（3）满
       * 目标块组（5），**块组（5）：间接块 29** （测试的单块组最大间接块数目为29）
       * 目标块组重新更新为（2），块组（2）满
       * 开始跨组分配，目标块组（4），**块组（4）：间接块 29**
       * 目标块组重新更新为（3），块组（3）满
       * 开始跨组分配，目标块组（5），**块组（5）：间接块 29**
       * 目标块组重新更新为（4），**块组（4）：间接块 26**，块组（4）满
       * 开始跨组分配，目标块组（6），**块组（6）：间接块 3**
       * 目标块组重新更新为（5），**块组（5）：间接块 12**
       * 大文件分块完成，达到预期运行结果

### FFS性能验证

#### 测试设计

本实验采用用户进程的方式运行测试程序，主要测试以下情况：

* 多文件读写性能
* 大文件读写性能
* 多目录乱序读写性能

#### 准备工作

读写性能最直观的指标是读写时间，而FTOS并没有直接获取时间或者运行时间的系统调用，因此为了统计时间，额外编写了228号系统调用，位于 `src/core/sysproc.h`，其代码如下所示：

```c
int sys_ctime() {
    u64 pct, frq;
    // 通过内联汇编获取开机后cpu运行周期
    asm volatile("mrs %[cnt], cntpct_el0" : [cnt] "=r"(pct));
    // 通过内联汇编获取cpu时钟频率
    asm volatile("mrs %[freq], cntfrq_el0" : [freq] "=r"(frq));
    // 计算开机时间，单位为毫秒
    return (int)(pct / (frq / 1000));
}
```

之后在同目录下的 `syscall.h` 中，修改系统调用表（`syscall_table` 与 `syscall_table_str`），如下所示：

```c
// syscall_table_str 也是类似的格式，不进行额外展示
int (*syscall_table[NR_SYSCALL])() = {[0 ... NR_SYSCALL - 1] = sys_default,
                                    [SYS_set_tid_address] = sys_gettid,
                                    [SYS_ioctl] = sys_ioctl,
                                    [SYS_gettid] = sys_gettid,
                                    [SYS_rt_sigprocmask] = sys_sigprocmask,
                                    [SYS_brk] = (int (*)())sys_brk,
                                    [SYS_execve] = sys_exec,
                                    [SYS_sched_yield] = sys_yield,
                                    [SYS_clone] = sys_clone,
                                    [SYS_wait4] = sys_wait4,
                                    [SYS_exit_group] = sys_exit,
                                    [SYS_exit] = sys_exit,
                                    [SYS_dup] = sys_dup,
                                    [SYS_chdir] = sys_chdir,
                                    [SYS_fstat] = sys_fstat,
                                    [SYS_newfstatat] = sys_fstatat,
                                    [SYS_mkdirat] = sys_mkdirat,
                                    [SYS_mknodat] = sys_mknodat,
                                    [87] = sys_unlink,  // 为实现rm新增87号系统调用
                                    [SYS_openat] = sys_openat,
                                    [SYS_writev] = (int (*)())sys_writev,
                                    [SYS_read] = (int (*)())sys_read,
                                    [SYS_write] = (int (*)())sys_write,
                                    [SYS_close] = sys_close,
                                    [SYS_myyield] = sys_yield,
                                    [228] = sys_ctime}; // 新增的228号系统调用
```

之后便可以通过syscall函数获取时间信息。

#### 测试代码

* 吞吐量测试

  * 单文件写：从小至大依次写文件，测量每毫秒写块数

    ```c
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
    ```

  * 单文件+大文件写：重复写大文件，测量每毫秒写块数

    ```c
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
    ```

* 延时

  * 多文件+大文件读写：重复写多个大文件，测量平均读写时间

    ```c
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
    ```

  * 多目录+多文件乱序读：在不同目录中乱序创建文件，之后乱序读取文件

    ```c
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
    ```

#### 测试结果

![test](pics/test.jpg)

![test1](pics/test1.png)
![test2](pics/test2.png)

![单文件](pics/单文件写测试.png)
![多文件](pics/多文件读写测试.png)

### shell功能验证

* echo

    `echo` 打印文本

    ```shell
    $ echo hello group 8
    hello group 8
    ```

* cat

    `cat` 打印文件内容

    ```shell
    $ echo hello group 8 > hello.txt
    $ cat hello.txt
    hello group 8
    ```

* mkdir

    `mkdir` 新建目录

    ```shell
    $ mkdir build
    $ ls
    .              16384 1 512
    ..             16384 1 512
    init           32768 2 13680
    sh             32768 3 38720
    echo           32768 4 30024
    cat            32768 5 30024
    mkdir          32768 6 30024
    ls             32768 7 34120
    test           32768 8 30264
    console        0 9 0
    testbuf        32768 10 71680
    hello.txt      32768 11 14
    build          16384 141 32
    $ 
    ```

* ls

    `ls` 查看当前目录下的文件以及子目录

    ```shell
    $ ls
    .              16384 1 512
    ..             16384 1 512
    init           32768 2 13680
    sh             32768 3 38720
    echo           32768 4 30024
    cat            32768 5 30024
    mkdir          32768 6 30024
    ls             32768 7 34120
    test           32768 8 30264
    console        0 9 0
    testbuf        32768 10 71680
    hello.txt      32768 11 14
    build          16384 141 32
    $ 
    ```

* rm

    `rm` 删除指定文件或空目录（目录不为空则失败）

    ```shell
    $ ls
    .              16384 1 512
    ..             16384 1 512
    init           32768 2 13680
    sh             32768 3 38720
    echo           32768 4 30024
    cat            32768 5 30024
    mkdir          32768 6 30024
    ls             32768 7 34120
    rm             32768 8 30024
    test           32768 9 30264
    console        0 10 0
    $ mkdir build
    $ cd build
    $ /echo a > a.txt
    $ /ls
    .              16384 121 48
    ..             16384 1 512
    a.txt          32768 122 2
    $ cd ..
    $ rm build
    rm build failed
    $ echo a > a.txt
    $ ls
    .              16384 1 512
    ..             16384 1 512
    init           32768 2 13680
    sh             32768 3 38720
    echo           32768 4 30024
    cat            32768 5 30024
    mkdir          32768 6 30024
    ls             32768 7 34120
    rm             32768 8 30024
    test           32768 9 30264
    console        0 10 0
    build          16384 121 48
    a.txt          32768 11 2
    $ rm a.txt
    $ ls
    .              16384 1 512
    ..             16384 1 512
    init           32768 2 13680
    sh             32768 3 38720
    echo           32768 4 30024
    cat            32768 5 30024
    mkdir          32768 6 30024
    ls             32768 7 34120
    rm             32768 8 30024
    test           32768 9 30264
    console        0 10 0
    build          16384 121 48
    $ 
    ```

## 遇到的问题与解决方法

### 问题一：Unexpected syscall

出现`Unexpected syscall`的报错，且程序编译正常。这是因为程序在运行时，陷入内核时没有找到正确的系统调用。系统调用由操作系统提供，每一个系统调用都对应一个系统调用号，libc库在陷入内核时，仅通过系统调用号来调用系统函数。

为了支持测试时的时间统计函数和shell中的rm命令，需要在系统中添加两个系统调用。

```c
#define NR_SYSCALL 512
int (*syscall_table[NR_SYSCALL])() = {[0 ... NR_SYSCALL - 1] = sys_default,
                                      [87] = sys_unlink,  
                                      [228] = sys_ctime};

int sys_ctime() {
    u64 pct, frq;
    asm volatile("mrs %[cnt], cntpct_el0" : [cnt] "=r"(pct));
    asm volatile("mrs %[freq], cntfrq_el0" : [freq] "=r"(frq));
    return (int)(pct / (frq / 1000));
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
```

### 问题二：num_bytes突变为0

原因是`inode.c`中的**inode_write**函数强制转换 `end` 为 `u16` 类型，溢出部分将强制截断，应该改为 `u32`，代码如下：

```c
if (end > entry->num_bytes) {
    entry->num_bytes = (u32)end;  // u16改为u32
    modified = true;
}
```

### 问题三：最大的inode_no无法分配

原始代码默认的inode分配区间实际上是 1 ~ num_inodes-1，而我们修改后的文件系统允许范围是 1 ~ num_inodes，因此需要修改一些断言，下面是一个例子：

```c
static Inode *inode_get(usize inode_no) {
    assert(inode_no > 0);
    assert(inode_no <= sblock->num_inodes);  // <改为<=
    acquire_spinlock(&lock);
    // ...
}
```

### 问题四：多次调用inodes.lookup

原始代码 `core/sysfile.c` 中 **create** 函数的定义中调用了两次 **inode.lookup** 函数，而这个函数实际上会修改传入的参数 **off**，也就是文件项在目录中的偏移位置，没有必要调用两次，修改后的代码如下所示：

```c
Inode *create(char *path, short type, short major, short minor, OpContext *ctx) {

    // ...

    // 文件名已存在
    if ((ino = inodes.lookup(dp, name, (usize *)&off)) != 0) {
        // printf("ino: %u\n", inodes.lookup(dp, name, (usize *)&off));
        ip = inodes.get(ino);
        // ...
    }
}
```

### 问题五：块大小修改问题

由于arena相关文件的限制，块大小无法修改为4096。修改块大小后，文件系统可以正常生成（mkfs正常工作），但是在进入文件系统的初始化阶段中sd初始化会出现问题，包括：

* 超级块无法正常读取
* EMMC send command error
* 其他错误

由于sd相关文件较多，没有定位到问题。

## 实验总结

通过这次实验，我们组的收获如下：

* 重新学习了操作系统的相关知识，对操作系统内核实现有了更加系统的了解
* 掌握了对文件系统的原理和实现方法，对xv6的简单文件系统以及快速文件系统深入了解
* 学会了对内核级代码的阅读、修改、调试、测试等工作
* 增加了团队之间的合作，对大型项目管理积累了宝贵的经验

同时这次实验难度较大，其中代码还出现了一些小问题和不够间接的地方，希望教学团队改进。
