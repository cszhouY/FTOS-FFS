#include <common/string.h>
#include <core/arena.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/sched.h>
#include <fs/inode.h>
#include <fs/used_block.h>

// this lock mainly prevents concurrent access to inode list `head`, reference
// count increment and decrement.
static SpinLock lock;
static ListNode head;

static const SuperBlock *sblock;
static const BlockCache *cache;
static Arena arena;

// 每个块组中的最大inode数目
#define GINODES (sblock->num_inodes / sblock->num_groups)
// 每个inode可在单个块组中分配的间接块数目，主要用于大文件处理
// 本实验中大文件的定义为超出了直接块数目
// 这里的分块数目算法核心是间隔分组，因此会有一个乘2的操作
#define NINBLOCKS_PER_GROUP ((BLOCK_SIZE / sizeof(u32)) / (NGROUPS - 1) * 2 + 1)  

extern u32 used_block[NGROUPS];

// return which block `inode_no` lives on.
static INLINE usize to_block_no(usize inode_no) {
    // inode所在的块编号 = 
    //      块组起始位置 + 块组偏移 + inode块组内偏移
    return sblock->bg_start + 
            sblock->blocks_per_group * ((inode_no - 1) / GINODES) + 
            (((inode_no - 1) % GINODES) / (INODE_PER_BLOCK));
}

// return the pointer to on-disk inode.
static INLINE InodeEntry *get_entry(Block *block, usize inode_no) {
    return ((InodeEntry *)block->data) + (inode_no % INODE_PER_BLOCK);
}

// return address array in indirect block.
static INLINE u32 *get_addrs(Block *block) {
    return ((IndirectBlock *)block->data)->addrs;
}

// used by `inode_map`.
static INLINE void set_flag(bool *flag) {
    if (flag != NULL)
        *flag = true;
}

// initialize inode tree.
void init_inodes(const SuperBlock *_sblock, const BlockCache *_cache) {
    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};

    init_spinlock(&lock, "inode tree");
    init_list_node(&head);
    sblock = _sblock;
    cache = _cache;
    init_arena(&arena, sizeof(Inode), allocator);

    if (ROOT_INODE_NO < sblock->num_inodes)
        inodes.root = inodes.get(ROOT_INODE_NO);
    else
        printf("(warn) init_inodes: no root inode.\n");
}

// initialize in-memory inode.
static void init_inode(Inode *inode) {
    init_spinlock(&inode->lock, "inode");
    init_rc(&inode->rc);
    init_list_node(&inode->node);
    inode->inode_no = 0;
    inode->valid = false;
}

// see `inode.h`.
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
    return 0;
}

// 修改思路
// 某一块组中的inode号集中，为了实现以下两点：
// 新建目录尽量在空闲块组中 && 同一父目录中的文件尽量在同一块组中
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

    if (type == INODE_REGULAR) {
        for (; gno < NGROUPS; gno++) {
            if (used_block[gno] < sblock->num_datablocks_per_group) {
                break;
            }
        }
        if (gno == NGROUPS)
            return 0;
    }

    // printf("inode_allog gno: %u\n", gno);

    // ino的含义变为某一块组内相对的inode编号
    // ino在块组内顺序分配
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
    // 这里表明分配失败
    return 0;
}  



// see `inode.h`.
static void inode_sync(OpContext *ctx, Inode *inode, bool do_write) {
    usize block_no = to_block_no(inode->inode_no);
    Block *block = cache->acquire(block_no);
    InodeEntry *entry = get_entry(block, inode->inode_no);

    if (inode->valid && do_write) {
        memcpy(entry, &inode->entry, sizeof(InodeEntry));
        cache->sync(ctx, block);
    } else if (!inode->valid) {
        memcpy(&inode->entry, entry, sizeof(InodeEntry));
        inode->valid = true;
    }

    cache->release(block);
}

// see `inode.h`.
static void inode_lock(Inode *inode) {
    assert(inode->rc.count > 0);
    acquire_spinlock(&inode->lock);

    if (!inode->valid)
        inode_sync(NULL, inode, false);
    assert(inode->entry.type != INODE_INVALID);
}

// see `inode.h`.
static void inode_unlock(Inode *inode) {
    assert(holding_spinlock(&inode->lock));
    assert(inode->rc.count > 0);
    release_spinlock(&inode->lock);
}

// see `inode.h`.
static Inode *inode_get(usize inode_no) {
    assert(inode_no > 0);
    assert(inode_no <= sblock->num_inodes);
    acquire_spinlock(&lock);

    Inode *inode = NULL;
    for (ListNode *cur = head.next; cur != &head; cur = cur->next) {
        Inode *inst = container_of(cur, Inode, node);
        if (inst->rc.count > 0 && inst->inode_no == inode_no) {
            increment_rc(&inst->rc);
            inode = inst;
            break;
        }
    }

    if (inode == NULL) {
        inode = alloc_object(&arena);
        assert(inode != NULL);
        init_inode(inode);
        inode->inode_no = inode_no;
        increment_rc(&inode->rc);
        merge_list(&head, &inode->node);
    }

    release_spinlock(&lock);

    return inode;
}

// see `inode.h`.
static void inode_clear(OpContext *ctx, Inode *inode) {
    InodeEntry *entry = &inode->entry;

    for (usize i = 0; i < INODE_NUM_DIRECT; i++) {
        usize addr = entry->addrs[i];
        if (addr != 0)
            cache->free(ctx, addr);
    }
    memset(entry->addrs, 0, sizeof(entry->addrs));

    usize iaddr = entry->indirect;
    if (iaddr != 0) {
        Block *block = cache->acquire(iaddr);
        u32 *addrs = get_addrs(block);
        for (usize i = 0; i < INODE_NUM_INDIRECT; i++) {
            if (addrs[i] != 0)
                cache->free(ctx, addrs[i]);
        }

        cache->release(block);
        cache->free(ctx, iaddr);
        entry->indirect = 0;
    }

    entry->num_bytes = 0;
    inode_sync(ctx, inode, true);
}

// see `inode.h`.
static Inode *inode_share(Inode *inode) {
    acquire_spinlock(&lock);
    increment_rc(&inode->rc);
    release_spinlock(&lock);
    return inode;
}

// see `inode.h`.
static void inode_put(OpContext *ctx, Inode *inode) {
    acquire_spinlock(&lock);
    bool is_last = inode->rc.count <= 1 && inode->entry.num_links == 0;

    if (is_last) {
        inode_lock(inode);
        release_spinlock(&lock);

        inode_clear(ctx, inode);
        inode->entry.type = INODE_INVALID;
        inode_sync(ctx, inode, true);

        inode_unlock(inode);
        acquire_spinlock(&lock);
    }

    if (decrement_rc(&inode->rc)) {
        detach_from_list(&inode->node);
        free_object(inode);
    }
    release_spinlock(&lock);
}

// this function is private to inode layer, because it can allocate block
// at arbitrary offset, which breaks the usual file abstraction.
//
// retrieve the block in `inode` where offset lives. If the block is not
// allocated, `inode_map` will allocate a new block and update `inode`, at
// which time, `*modified` will be set to true.
// the block number is returned.
//
// NOTE: caller must hold the lock of `inode`.

// 修改块分配逻辑，增加块组编号作为分配依据
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

// see `inode.h`.
static usize inode_read(Inode *inode, u8 *dest, usize offset, usize count) {
    // printf("> in inode_read\n");
    InodeEntry *entry = &inode->entry;

    if (inode->entry.type == INODE_DEVICE) {
        // printf("in inode_read Device\n");
        assert(inode->entry.major == 1);
        return (usize)console_read(inode, (char *)dest, (isize)count);
    }
    if (count + offset > entry->num_bytes)
        count = entry->num_bytes - offset;
    usize end = offset + count;

    // printf("& inode_read from inode %u, offset %u, size %d \n", inode->inode_no, offset, count);
    assert(offset <= entry->num_bytes);

    assert(end <= entry->num_bytes);
    assert(offset <= end);

    // printf("start inode_read\n");
    usize step = 0;
    for (usize begin = offset; begin < end; begin += step, dest += step) {
        bool modified = false;
        usize block_no = inode_map(NULL, inode, begin, &modified);
        assert(!modified);

        Block *block = cache->acquire(block_no);
        usize index = begin % BLOCK_SIZE;
        step = MIN(end - begin, BLOCK_SIZE - index);
        memmove(dest, block->data + index, step);
        cache->release(block);
    }
    // printf("& inode_read from inode %u, offset %u, size %d \n", inode->inode_no, offset, count);
    return count;
}

// see `inode.h`.
static usize inode_write(OpContext *ctx, Inode *inode, u8 *src, usize offset, usize count) {
    InodeEntry *entry = &inode->entry;
    usize end = offset + count;
    if (inode->entry.type == INODE_DEVICE) {
        assert(inode->entry.major == 1);
        return (usize)console_write(inode, (char *)src, (isize)count);
    }
    // printf ("inode_write: %u, num_bytes: %u, offset: %llu, count: %llu\n", inode->inode_no, entry->num_bytes, offset, count);
    assert(offset <= entry->num_bytes);
    assert(end <= INODE_MAX_BYTES);
    assert(offset <= end);

    usize step = 0;
    bool modified = false;
    for (usize begin = offset; begin < end; begin += step, src += step) {
        usize block_no = inode_map(ctx, inode, begin, &modified);
        Block *block = cache->acquire(block_no);
        usize index = begin % BLOCK_SIZE;
        step = MIN(end - begin, BLOCK_SIZE - index);
        memmove(block->data + index, src, step);
        cache->sync(ctx, block);
        cache->release(block);
    }

    if (end > entry->num_bytes) {
        entry->num_bytes = (u32)end;
        modified = true;
    }
    if (modified)
        inode_sync(ctx, inode, true);

    // printf("inode_write to inode %u, offset %u, size %d \n", inode->inode_no, offset, count);
    return count;
}

// see `inode.h`.
static usize inode_lookup(Inode *inode, const char *name, usize *index) {
    InodeEntry *entry = &inode->entry;
    // printf("inode %u, name %s, inode_type %u\n", inode->inode_no, name, inode->entry.type);
    assert(entry->type == INODE_DIRECTORY);

    DirEntry dentry;
    for (usize offset = 0; offset < entry->num_bytes; offset += sizeof(dentry)) {
        inode_read(inode, (u8 *)&dentry, offset, sizeof(dentry));
        if (dentry.inode_no != 0 && strncmp(name, dentry.name, FILE_NAME_MAX_LENGTH) == 0) {
            if (index != NULL)
                *index = offset / sizeof(dentry);
            return dentry.inode_no;
        }
    }

    return 0;
}

static usize inode_empty(Inode *inode) {
    InodeEntry *entry = &inode->entry;
    // printf("inode %u, name %s, inode_type %u\n", inode->inode_no, name, inode->entry.type);
    assert(entry->type == INODE_DIRECTORY);

    DirEntry dentry;
    for (usize offset = 2 * sizeof(dentry); offset < entry->num_bytes; offset += sizeof(dentry)) {
        if (inode_read(inode, (u8 *)&dentry, offset, sizeof(dentry)) != sizeof(dentry))
            PANIC("inode_empty");
        if (dentry.inode_no != 0)
            return 0;
    }
    return 1;
}



// see `inode.h`.
static usize inode_insert(OpContext *ctx, Inode *inode, const char *name, usize inode_no) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);

    DirEntry dentry;
    usize offset = 0;
    for (; offset < entry->num_bytes; offset += sizeof(dentry)) {
        inode_read(inode, (u8 *)&dentry, offset, sizeof(dentry));
        if (dentry.inode_no == 0)
            break;
    }

    dentry.inode_no = (u16)inode_no;
    strncpy(dentry.name, name, FILE_NAME_MAX_LENGTH);
    inode_write(ctx, inode, (u8 *)&dentry, offset, sizeof(dentry));
    return offset / sizeof(dentry);
}

// see `inode.h`.
static void inode_remove(OpContext *ctx, Inode *inode, usize index) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);

    DirEntry dentry;
    usize offset = index * sizeof(dentry);
    if (offset >= entry->num_bytes)
        return;

    memset(&dentry, 0, sizeof(dentry));
    inode_write(ctx, inode, (u8 *)&dentry, offset, sizeof(dentry));
}

/* Paths. */

/* Copy the next path element from path into name.
 *
 * Return a pointer to the element following the copied one.
 * The returned path has no leading slashes,
 * so the caller can check *path=='\0' to see if the name is the last one.
 * If no name to remove, return 0.
 *
 * Examples:
 *   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
 *   skipelem("///a//bb", name) = "bb", setting name = "a"
 *   skipelem("a", name) = "", setting name = "a"
 *   skipelem("", name) = skipelem("////", name) = 0
 */
static const char *skipelem(const char *path, char *name) {
    const char *s;
    int len;

    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = (int)(path - s);
    if (len >= FILE_NAME_MAX_LENGTH)
        memmove(name, s, FILE_NAME_MAX_LENGTH);
    else {
        memmove(name, s, (usize)len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

/* Look up and return the inode for a path name.
 *
 * If parent != 0, return the inode for the parent and copy the final
 * path element into name, which must have room for DIRSIZ bytes.
 * Must be called inside a transaction since it calls iput().
 */
static Inode *namex(const char *path, int nameiparent, char *name, OpContext *ctx) {
    Inode *ip, *next;

    if (*path == '/')
        ip = inodes.get(1);
    else
        ip = inodes.share(thiscpu()->proc->cwd);

    while ((path = skipelem(path, name)) != 0) {
        inodes.lock(ip);
        if (ip->entry.type != INODE_DIRECTORY) {
            inodes.unlock(ip);
            bcache.begin_op(ctx);
            inodes.put(ctx, ip);
            bcache.end_op(ctx);
            return 0;
        }
        if (nameiparent && *path == '\0') {
            // Stop one level early.
            inodes.unlock(ip);
            return ip;
        }
        if (*path == '.' && *(path + 1) == '\0') {
            inodes.unlock(ip);
            return ip;
        }
        if (inodes.lookup(ip, name, 0) == 0) {
            inodes.unlock(ip);
            inodes.put(ctx, ip);
            return 0;
        }
        next = inodes.get(inodes.lookup(ip, name, 0));
        inodes.unlock(ip);
        inodes.put(ctx, ip);
        ip = next;
    }
    if (nameiparent) {
        bcache.begin_op(ctx);
        inodes.put(ctx, ip);
        bcache.end_op(ctx);
        return 0;
    }
    return ip;
}

Inode *namei(const char *path, OpContext *ctx) {
    char name[FILE_NAME_MAX_LENGTH];
    return namex(path, 0, name, ctx);
}

Inode *nameiparent(const char *path, char *name, OpContext *ctx) {
    return namex(path, 1, name, ctx);
}

/*
 * Copy stat information from inode.
 * Caller must hold ip->lock.
 */
void stati(Inode *ip, struct stat *st) {
    // FIXME: support other field in stat
    st->st_dev = 1;
    st->st_ino = ip->inode_no;
    st->st_nlink = ip->entry.num_links;
    st->st_size = ip->entry.num_bytes;

    switch (ip->entry.type) {
        case INODE_REGULAR: st->st_mode = S_IFREG; break;
        case INODE_DIRECTORY: st->st_mode = S_IFDIR; break;
        case INODE_DEVICE: st->st_mode = 0; break;
        default: PANIC("unexpected stat type %d. ", ip->entry.type);
    }
}
InodeTree inodes = {
    .alloc = inode_alloc,
    .allocg = inode_alloc_group, // 修改为inode_alloc_group
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
    .empty = inode_empty,
    .insert = inode_insert,
    .remove = inode_remove,
};
