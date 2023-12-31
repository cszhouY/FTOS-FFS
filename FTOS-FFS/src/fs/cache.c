#include <common/bitmap.h>
#include <common/string.h>
#include <core/arena.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/proc.h>
#include <fs/cache.h>
#include <fs/used_block.h>

static const SuperBlock *sblock;
static const BlockDevice *device;

static SpinLock lock;     // protects block cache.
static Arena arena;       // memory pool for `Block` struct.
static ListNode head;     // the list of all allocated in-memory block.
static LogHeader header;  // in-memory copy of log header block.

static usize last_allocated_ts;  // last timestamp assigned by `begin_op`.
static usize last_persisted_ts;  // last timestamp of atomic operation that is persisted to disk.

static usize log_start;  // the block number that is next to the log header block.
static usize log_size;   // maximum number of blocks that can be recorded in log.
static usize log_used;   // number of log entries reserved/used by uncommitted atomic operations.

static usize op_count;  // number of outstanding atomic operations that are not ended by `end_op`.

// first defined here
u32 used_block[NGROUPS] = {0};

// read the content from disk.
static INLINE void device_read(Block *block) {
    device->read(block->block_no, block->data);
}

// write the content back to disk.
static INLINE void device_write(Block *block) {
    device->write(block->block_no, block->data);
}

// read log header from disk.
static INLINE void read_header() {
    device->read(sblock->log_start, (u8 *)&header);
}

// write log header back to disk.
static INLINE void write_header() {
    device->write(sblock->log_start, (u8 *)&header);
}

static void replay();

// initialize block cache.
void init_bcache(const SuperBlock *_sblock, const BlockDevice *_device) {
    sblock = _sblock;
    device = _device;

    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};
    init_spinlock(&lock, "block cache");
    init_arena(&arena, sizeof(Block), allocator);
    init_list_node(&head);

    last_allocated_ts = 0;
    last_persisted_ts = 0;

    log_start = sblock->log_start + 1;
    log_size = MIN(sblock->num_log_blocks - 1, LOG_MAX_SIZE);
    log_used = 0;

    op_count = 0;

    read_header();
    replay();
}

// initialize a block struct.
static void init_block(Block *block) {
    block->block_no = 0;
    init_list_node(&block->node);
    block->acquired = false;
    block->pinned = false;

    init_sleeplock(&block->lock, "block");
    block->valid = false;
    memset(block->data, 0, sizeof(block->data));
}

static usize _get_num_cached_blocks() {
    usize count = 0;
    for (ListNode *x = head.next; x != &head; x = x->next) {
        count++;
    }
    return count;
}

// see `cache.h`.
static usize get_num_cached_blocks() {
    acquire_spinlock(&lock);
    usize count = _get_num_cached_blocks();
    release_spinlock(&lock);
    return count;
}

// see `cache.h`.
static Block *cache_acquire(usize block_no) {
    acquire_spinlock(&lock);

    Block *slot = NULL;
    for (ListNode *cur = head.next; cur != &head; cur = cur->next) {
        Block *block = container_of(cur, Block, node);
        if (block->block_no == block_no) {
            slot = block;
            break;
        }

        // find a candidate to be evicted.
        if (!block->acquired && !block->pinned)
            slot = block;
    }

    if (slot && slot->block_no != block_no) {
        if (_get_num_cached_blocks() >= EVICTION_THRESHOLD) {
            slot->block_no = block_no;
            slot->valid = false;
        } else {
            // no need to evict. Just allocate a new one.
            slot = NULL;
        }
    }

    // when not found and no block is going to be evicted.
    if (!slot) {
        slot = alloc_object(&arena);
        assert(slot != NULL);
        init_block(slot);
        slot->block_no = block_no;
    }

    // move to the front.
    detach_from_list(&slot->node);
    merge_list(&head, &slot->node);

    // NOTE: set `acquired` before releasing cache lock to prevent someone evicting
    // this block in the window between `release` and `acquire`.
    slot->acquired = true;

    release_spinlock(&lock);
    acquire_sleeplock(&slot->lock);

    if (!slot->valid) {
        device_read(slot);
        slot->valid = true;
    }

    return slot;
}

// see `cache.h`.
static void cache_release(Block *block) {
    release_sleeplock(&block->lock);
    acquire_spinlock(&lock);
    block->acquired = false;
    release_spinlock(&lock);
}

// see `cache.h`.
static void cache_begin_op(OpContext *ctx) {
    init_spinlock(&ctx->lock, "atomic operation context");
    ctx->ts = 0;
    ctx->num_blocks = 0;
    memset(ctx->block_no, 0, sizeof(ctx->block_no));

    acquire_spinlock(&lock);

    while (log_used + OP_MAX_NUM_BLOCKS > log_size) {
        sleep(&log_used, &lock);
    }

    log_used += OP_MAX_NUM_BLOCKS;
    ctx->ts = ++last_allocated_ts;
    op_count++;

    release_spinlock(&lock);
}

// see `cache.h`.
static void cache_sync(OpContext *ctx, Block *block) {
    if (ctx) {
        acquire_spinlock(&ctx->lock);

        usize i = 0;
        for (; i < ctx->num_blocks; i++) {
            if (ctx->block_no[i] == block->block_no)
                break;
        }

        assert(i < OP_MAX_NUM_BLOCKS);
        ctx->block_no[i] = block->block_no;
        if (i >= ctx->num_blocks)
            ctx->num_blocks++;

        release_spinlock(&ctx->lock);
        acquire_spinlock(&lock);
        block->pinned = true;
        release_spinlock(&lock);
    } else
        device_write(block);
}

// commit all block number records associated with `ctx` into log header.
//
// NOTE: the caller must hold the lock of block cache.
static void commit(OpContext *ctx) {
    acquire_spinlock(&ctx->lock);

    usize absorbed = 0;
    for (usize i = 0; i < ctx->num_blocks; i++) {
        usize block_no = ctx->block_no[i];
        usize j = 0;
        for (; j < header.num_blocks; j++) {
            if (header.block_no[j] == block_no)
                break;
        }

        assert(j < log_size);
        header.block_no[j] = block_no;
        if (j < header.num_blocks)
            absorbed++;
        else
            header.num_blocks++;
    }

    release_spinlock(&ctx->lock);

    // blocks reserved but not used are now returned back.
    usize unused = OP_MAX_NUM_BLOCKS - ctx->num_blocks;

    // the lock is holded by the caller.
    log_used -= unused + absorbed;
    wakeup(&log_used);
}

// replay logs if there's any.
static void replay() {
    if (header.num_blocks == 0)
        return;

    // step 3: copy blocks from log to their original locations on disk.
    // don't forget to un-pin them in block cache.
    for (usize i = 0; i < header.num_blocks; i++) {
        Block *src = cache_acquire(log_start + i);
        Block *dest = cache_acquire(header.block_no[i]);
        memcpy(dest->data, src->data, BLOCK_SIZE);
        cache_release(src);
        device_write(dest);
        acquire_spinlock(&lock);
        dest->pinned = false;
        release_spinlock(&lock);
        cache_release(dest);
    }

    // step 4: now that all blocks are written back, just clear log.
    header.num_blocks = 0;
    write_header();
}

// persist all blocks recorded in log header to disk.
//
// NOTE: checkpointing is time consuming, so the caller should NOT hold the
// lock of block cache.
static void checkpoint() {
    // step 1: write blocks into logging area first.
    for (usize i = 0; i < header.num_blocks; i++) {
        Block *src = cache_acquire(header.block_no[i]);
        Block *dest = cache_acquire(log_start + i);
        memcpy(dest->data, src->data, BLOCK_SIZE);
        cache_release(src);
        device_write(dest);
        cache_release(dest);
    }

    // step 2: write header block to mark all atomic operations are now
    // persisted in log.
    write_header();

    // step 3 & step 4 in `replay`.
    replay();

    // step 5: wake up all sleeping threads waiting in `end_op`.
    acquire_spinlock(&lock);
    last_persisted_ts = last_allocated_ts;
    release_spinlock(&lock);
    wakeup(&last_persisted_ts);
}

// see `cache.h`.
static void cache_end_op(OpContext *ctx) {
    acquire_spinlock(&lock);

    commit(ctx);
    op_count--;

    // this will make sure only one thread gets `do_checkpoint == true`.
    bool do_checkpoint = op_count == 0;
    usize log_size_copy = log_size;

    if (do_checkpoint) {
        // this will block all `begin_op`.
        log_size = 0;
    } else {
        while (ctx->ts > last_persisted_ts) {
            sleep(&last_persisted_ts, &lock);
        }
    }

    release_spinlock(&lock);

    if (do_checkpoint) {
        // at this time:
        // 1. all atomic operations, except this one, are waiting in `end_op`.
        //    idealy, no one will invoke `cache_sync`.
        // 2. all `begin_op` will be blocked because `log_size` is zero.
        // 3. some read-only operations can continue to acquire the lock of
        //    block cache.

        checkpoint();

        acquire_spinlock(&lock);
        log_size = log_size_copy;
        log_used = 0;
        release_spinlock(&lock);
        wakeup(&log_used);
    }
}

// static usize cache_alloc(OpContext *ctx) {
//     for (usize i = 0; i < sblock->num_blocks; i += BIT_PER_BLOCK) {
//         usize block_no = sblock->bitmap_start + (i / BIT_PER_BLOCK);
//         Block *block = cache_acquire(block_no);

//         BitmapCell *bitmap = (BitmapCell *)block->data;
//         for (usize j = 0; j < BIT_PER_BLOCK && i + j < sblock->num_blocks; j++) {
//             if (!bitmap_get(bitmap, j)) {
//                 bitmap_set(bitmap, j);
//                 cache_sync(ctx, block);
//                 cache_release(block);

//                 block_no = i + j;
//                 block = cache_acquire(block_no);
//                 memset(block->data, 0, BLOCK_SIZE);
//                 cache_sync(ctx, block);
//                 cache_release(block);
//                 return block_no;
//             }
//         }

//         cache_release(block);
//     }

//     PANIC("cache_alloc: no free block");
// }


// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.

// 修改：适应FFS文件结构
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

// 新增cache_allocg函数，用于在特定块组内分配数据块
static usize cache_allocg(OpContext *ctx, u32 gno) {
    assert(gno < NGROUPS);
    for (usize i = 0; i < sblock->blocks_per_group; i += BIT_PER_BLOCK) {
        usize block_no = sblock->bg_start + 
                         sblock->bitmap_start_per_group + gno * sblock->blocks_per_group +
                         i / BIT_PER_BLOCK;
        Block *block = cache_acquire(block_no);

        BitmapCell *bitmap = (BitmapCell *)block->data;
        for (usize j = 0; j < BIT_PER_BLOCK && j < sblock->blocks_per_group; j++) {
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

// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
static void cache_free(OpContext *ctx, usize block_no) {
    usize h = (block_no - sblock->bg_start) / sblock->blocks_per_group;
    usize off = (block_no - sblock->bg_start) % sblock->blocks_per_group;
    usize i = off / BIT_PER_BLOCK, j = off % BIT_PER_BLOCK;
    Block *block = cache_acquire(sblock->bg_start + h * sblock->blocks_per_group +
                                 sblock->bitmap_start_per_group + i);

    BitmapCell *bitmap = (BitmapCell *)block->data;
    assert(bitmap_get(bitmap, j));
    bitmap_clear(bitmap, j);
    used_block[h]--;

    cache_sync(ctx, block);
    cache_release(block);
}

BlockCache bcache = {
    .get_num_cached_blocks = get_num_cached_blocks,
    .acquire = cache_acquire,
    .release = cache_release,
    .begin_op = cache_begin_op,
    .sync = cache_sync,
    .end_op = cache_end_op,
    .alloc = cache_alloc,
    .allocg = cache_allocg,
    .free = cache_free,
};
