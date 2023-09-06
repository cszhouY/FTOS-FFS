#include <driver/sd.h>
#include <fs/block_device.h>

// TODO: we should read this value from MBR block.
#define BLOCKNO_OFFSET (0x20800)

static void sd_read(usize block_no, u8 *buffer) {
    struct buf b;
    b.blockno = (u32)block_no + BLOCKNO_OFFSET;
    b.flags = 0;
    sdrw(&b);
    memcpy(buffer, b.data, sizeof(b.data));
}

static void sd_write(usize block_no, u8 *buffer) {
    struct buf b;
    b.blockno = (u32)block_no + BLOCKNO_OFFSET;
    b.flags = B_DIRTY | B_VALID;
    memcpy(b.data, buffer, sizeof(b.data));
    sdrw(&b);
}

static u8 sblock_data[BLOCK_SIZE];
BlockDevice block_device;

void init_block_device() {
    sd_init();

    usize sd_num;
    usize sd_size = (sizeof(struct buf) - 16);
    usize sd_start = SECTS_PER_BLOCK;

    for (sd_num = 0; sd_num < BLOCK_SIZE / sd_size; sd_num++) {
        sd_read(sd_start + sd_num, sblock_data + sd_num * sd_size);
    }
    block_device.read = sd_read;
    block_device.write = sd_write;
}

const SuperBlock *get_super_block() {
    return (const SuperBlock *)sblock_data;
}
