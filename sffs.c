#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "params.h"

typedef unsigned long long ull;

void * blocks[BLOCK_NUM];

void * new_block() {
    void * ret;
    assert ( (ret = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) != MAP_FAILED );
    memset(ret, 0, BLOCK_SIZE);
    return ret;
}

void delete_block(void * ptr) {
    assert ( munmap(ptr, BLOCK_SIZE) != -1 );
}

static void *sffs_init(struct fuse_conn_info *conn) {
    // Fill block 0, ie. the summary of this file system
    blocks[0] = new_block();
    ((ull*)blocks[0])[0] = BLOCK_SIZE;
    ((ull*)blocks[0])[1] = BLOCK_NUM;
    ((ull*)blocks[0])[2] = 1 + BMBLOCK_NUM + IBLOCK_NUM;

    // Fill bitmap blocks
    for (ull i = 0; i < BMBLOCK_NUM; ++i) {
        blocks[BMBLOCK_BEGIN + i] = new_block();
    }

    // Fill index blocks
    for (ull i = 0; i < IBLOCK_NUM; ++i) {
        blocks[IBLOCK_BEGIN + i] = new_block();
    }

    return NULL;
}

static int sffs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    return 0;
}

static const struct fuse_operations op = {
    .init = sffs_init,
    .readdir = sffs_readdir,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &op, NULL);
}
