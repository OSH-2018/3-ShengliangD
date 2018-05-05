#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fuse.h>
#include <sys/mman.h>

#include "params.h"

void * media[BLOCK_NUM];

static void *sffs_init(struct fuse_conn_info *conn) {
    // Fill block 0, ie. the summary of this file system
    //
    // Fill bitmap blocks
    //
    // Fill index blocks
}

static const struct fuse_operations op = {
    .init = sffs_init,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &op, NULL);
}
