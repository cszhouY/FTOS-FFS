#include <fs/block_device.h>
#include <fs/cache.h>
#include <fs/defines.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <fs/used_block.h>
#include <common/bitmap.h>

static u8 used_block_data[BLOCK_SIZE];
extern u32 used_block[NGROUPS];

void init_filesystem() {
    init_block_device();
    // printf("init_block_device finished.\n");
    const SuperBlock *sblock = get_super_block();
    init_bcache(sblock, &block_device);

    u32 i, j, h;
    for (h = 0; h < NGROUPS; h++) {
        for (i = 0; i < sblock->blocks_per_group; i += BIT_PER_BLOCK) {
            block_device.read(sblock->bg_start + 
                              h * sblock->blocks_per_group + 
                              sblock->bitmap_start_per_group +
                              i / BIT_PER_BLOCK, used_block_data);
            BitmapCell *bitmap = (BitmapCell *)used_block_data;
            for (j = sblock->data_start_per_group; i + j < BIT_PER_BLOCK && j < sblock->blocks_per_group; j++) {
                if (bitmap_get(bitmap, j))  {
                    used_block[h]++;
                }
            }
        }
        printf("used_block: %u: %u\n", h, used_block[h]);
    }
    // printf("init_bcache finished.\n");
    init_inodes(sblock, &bcache);
    // printf("init_inodes finished.\n");
}
