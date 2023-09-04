# FTOS-FFS设计与实验报告

## 实验目标

* 实现块组磁盘结构
* 实现文件与目录的放置策略
* 实现大文件的优化放置策略
* shell程序编写

## 实验原理

### 快速文件系统（FFS）

快速文件系统

### shell

shell

## 实验内容

### 实现块组磁盘结构

* 主要修改文件：`src/fs/defines.h`，`src/user/mkfs/main.c`
* 修改说明：

    1. 修改`defines.h`中的超级块结构，新的超级块结构如下所示：

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

    2. 修改`defines.h`中相关宏定义，主要包括以下内容：

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

    3. 修改`mkfs/main.c`中相关宏定义，主要包括以下内容：

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
            #define IBLOCK(i, sb) (sb.bg_start + BPG * (i / NIPG) + (i % NIPG) / IPB)
            // 根据inode编号获取其所在的块组编号
            #define IGROUP(i, sb) ((i - 1) / NIPG)
            // 根据块编号获取其所在的块组编号
            #define BGROUP(b, sb) ((b - 2 - LOG_MAX_SIZE) / BPG)
            ```

    4. 修改`mkfs/main.c`相关变量定义，主要包括以下内容：

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
            uint used_block[NGROUPS];
            ```

    5. 修改`mkfs/main.c`中相关函数声明以及定义，如下所示：

        * 新增长度为块组数目的数组，用于存储块组内部数据块使用数目，便于位图与用户程序写入磁盘时定位到空闲块

            ```c
            // 这一数组仅在mkfs中生效，文件系统中会重新定义类似变量以供相关函数调用
            uint used_block[NGROUPS];
            ```

### 实现文件/目录放置策略

* 主要修改文件
* 修改说明

### 实现大文件优化放置策略

* 主要修改文件
* 修改说明

### shell程序编写

#### echo

* 基本功能
* 代码说明

#### cat

* 基本功能
* 代码说明

#### mkdir

* 基本功能
* 代码说明

#### ls

* 基本功能
* 代码说明

## 实验验证

### 测试程序编写

### 测试结果

## 实验总结
