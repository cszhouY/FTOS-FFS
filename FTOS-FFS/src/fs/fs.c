#include <fs/block_device.h>
#include <fs/cache.h>
#include <fs/defines.h>
#include <fs/fs.h>
#include <fs/inode.h>

void init_filesystem() {
    init_block_device();
    // printf("init_block_device finished.\n");
    const SuperBlock *sblock = get_super_block();
    init_bcache(sblock, &block_device);
    // printf("sblock: %s\n", (char *)sblock);
    // printf("sblock->num_inodes: %u\n", sblock->num_inodes);
    // printf("init_bcache finished.\n");
    init_inodes(sblock, &bcache);
    // printf("init_inodes finished.\n");
}
