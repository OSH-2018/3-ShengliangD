#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <assert.h>
#include <time.h>

#define min(a, b) ((a)<=(b)?(a):(b))
#define max(a, b) ((a)>=(b)?(a):(b))

#include "sffs_def.h"
#include "sffs_blocks.h"

static void *sffs_init(struct fuse_conn_info *conn) {
    // Fill block 0, ie. the summary of this file system
    blocks[0] = new_block();

    ((ull*) blocks[0])[0] = BLOCK_SIZE;
    ((ull*) blocks[0])[1] = BLOCK_NUM;
    ((ull*) blocks[0])[2] = 1 + BMBLOCK_NUM + IBLOCK_NUM;

    // Fill bitmap blocks
    for (ull i = 0; i < BMBLOCK_NUM; ++i) {
        blocks[BMBLOCK_BEGIN + i] = new_block();
    }

    // Fill index blocks
    for (ull i = 0; i < IBLOCK_NUM; ++i) {
        blocks[IBLOCK_BEGIN + i] = new_block();
    }

    // Mark all these blocks above
    mark_block(0);
    for (ull i = 0; i < BMBLOCK_NUM; ++i) {
        mark_block(BMBLOCK_BEGIN + i);
    }
    for (ull i = 0; i < IBLOCK_NUM; ++i) {
        mark_block(IBLOCK_BEGIN + i);
    }

    // Fill the super block
    {
        super_block_t * sb = (super_block_t*)blocks[0];
        sb->total_blocks = BLOCK_NUM;
        sb->used_blocks = 1 + BMBLOCK_NUM + IBLOCK_NUM;
    }

    return NULL;
}

// For now, we have only a root, "/"
static int sffs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    // Fill the traditional entries
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    // Fill the entries that really exist
    for (ull i = 0; i < IBLOCK_NUM; ++i) {
        for (ull j = 0; j < BLOCK_SIZE / sizeof (ull); ++j) {
            int bid = ((ull*)blocks[IBLOCK_BEGIN + i])[j];
            if (bid != 0) {
                const attr_block_t * attr = (attr_block_t*)blocks[bid];

                struct stat st;
                memset(&st, 0, sizeof(st));
                st.st_mode = S_IFREG | 0755;
                st.st_atime = st.st_mtime = st.st_ctime = attr->atime;
                st.st_uid = fuse_get_context()->uid;
                st.st_gid = fuse_get_context()->gid;
                st.st_nlink = 1;

                filler(buf, attr->name, &st, 0);
            }
        }
    }

    return 0;
}

static int sffs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_uid = fuse_get_context()->uid;
        stbuf->st_gid = fuse_get_context()->gid;

        return 0;
    } else {
        // Find the first block of the file, which contains attributes
        attr_block_t * ab;
        if (find_attr_block(path, NULL, NULL, &ab) != 0)
            return -ENOENT;

        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFREG | 0755;
        stbuf->st_atime = ab->atime;
        stbuf->st_mtime = ab->mtime;
        stbuf->st_uid = fuse_get_context()->uid;
        stbuf->st_gid = fuse_get_context()->gid;
        stbuf->st_nlink = 1;
        stbuf->st_size = ab->size;

        return 0;
    }
    return -ENOENT;
}

static int sffs_mknod(const char *path, mode_t mode, dev_t dev)
{
    ull bid = alloc_block();

    // Add index info
    {
        ull iid = 0;
        while (((ull*)blocks[IBLOCK_BEGIN + iid / INDEX_PER_BLOCK])[iid % INDEX_PER_BLOCK] != 0)
            ++iid;
        assert ( iid != INDEX_PER_BLOCK * IBLOCK_NUM );
        ((ull*)blocks[IBLOCK_BEGIN + iid / INDEX_PER_BLOCK])[iid % INDEX_PER_BLOCK] = bid;
    }

    // Fill file attr into the c
    attr_block_t * attr = (attr_block_t*)blocks[bid];
    strncpy(attr->name, path + 1, FNAME_LIMIT + 1);
    time(&attr->atime);
    time(&attr->mtime);
    attr->size = 0;
    attr->chain.prev = bid;
    attr->chain.next = 0;

    return 0;
}

static int sffs_open(const char *path, struct fuse_file_info *fi) {
    if (find_attr_block(path, NULL, NULL, NULL) != 0)
        return -ENOENT;
    else
        return 0;
}

// TODO: offset ignored for now
static int sffs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Find the attr block
    attr_block_t * ab;
    if (find_attr_block(path, NULL, NULL, &ab) != 0)
        return -ENOENT;
    time(&ab->atime);

    seek_tuple_t st;
    locate(offset, ab, &st);

    size_t seek;
    for (seek = 0; seek < size && offset + seek < ab->size; ) {
        next_byte(&st, 0);
        chain_block_t * cb = (chain_block_t*)blocks[st.chain_block_id];        

        void * copy_src = blocks[cb->data_block_ids[st.chain_block_seek]] + st.data_block_seek;
        ull copy_size = min(ab->size - seek, min(size - seek, BLOCK_SIZE - st.data_block_seek));
        memcpy(buf + seek, copy_src, copy_size);
        seek += copy_size;
        st.data_block_seek += copy_size - 1;
    }

    return seek;
}

// TODO: Assume offset is valid for now
static int sffs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Find the attr block
    ull bid;
    attr_block_t * ab;
    if (find_attr_block(path, NULL, &bid, &ab) != 0)
        return -ENOENT;
    
    // Judge if there is enough space
    {
        super_block_t * sb = (super_block_t*)blocks[0];
        ull avail_blocks = sb->total_blocks - sb->used_blocks;
        if (size + offset > ab->size) {
            ull need_blocks = (offset + size + BLOCK_SIZE - 1) / BLOCK_SIZE - (ab->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
            if (need_blocks > avail_blocks)
                return -E2BIG;
        }
    }

    time(&ab->mtime);

    seek_tuple_t st;
    locate(offset, ab, &st);

    size_t seek;
    for (seek = 0; seek < size; ) {
        next_byte(&st, 1);
        chain_block_t * cb = (chain_block_t*)blocks[st.chain_block_id];

        void * copy_dst = blocks[cb->data_block_ids[st.chain_block_seek]] + st.data_block_seek;
        ull copy_size = min(size - seek, BLOCK_SIZE - st.data_block_seek);
        memcpy(copy_dst, buf + seek, copy_size);
        seek += copy_size;
        st.data_block_seek += copy_size - 1;
    }

    ab->size = max(ab->size, min(ab->size, offset) + size);
    time(&ab->atime);    

    return seek;
}

static int sffs_unlink(const char *path) {
    ull iid, bid;
    attr_block_t *ab;
    if (find_attr_block(path, &iid, &bid, &ab) != 0)
        return -ENOENT;

    // Delete in index
    ((ull*)(blocks[IBLOCK_BEGIN + iid / (BLOCK_SIZE / sizeof(ull))]))[iid % (BLOCK_SIZE / sizeof(ull))] = 0;

    seek_tuple_t st;
    locate(0, ab, &st);

    // Free all the blocks
    free_space(st);
    free_block(bid);

    return 0;
}

static int sffs_truncate(const char *path, off_t size) {
    // Locate the file
    attr_block_t * ab;
    if (find_attr_block(path, NULL, NULL, &ab) != 0)
        return -ENOENT;
    time(&ab->mtime);

    if (ab->size == size)
        return 0;

    if (ab->size > size) {  // If the file is larger, locate the seek and remove later blocks
        seek_tuple_t st;
        locate(size, ab, &st);
        free_space(st);
    } else {  // The file is smaller, extent it and fill with zero
        // I'm really lazy, use this inefficient method for now.
        {
            char * zeros = (char*)malloc(size - ab->size);
            memset(zeros, 0, size - ab->size);
            sffs_write(path, zeros, size - ab->size, ab->size, NULL);
            free(zeros);
        }
    }
    ab->size = size;

    return 0;
}

static int sffs_statfs(const char * path, struct statvfs *st) {
    super_block_t * sb = (super_block_t*)blocks[0];

    st->f_bsize = BLOCK_SIZE;
    st->f_frsize = BLOCK_SIZE;
    st->f_blocks = sb->total_blocks;
    st->f_bfree = sb->total_blocks - sb->used_blocks;
    st->f_bavail = sb->total_blocks - sb->used_blocks;

    return 0;
}

static const struct fuse_operations op = {
    .init = sffs_init,
    .readdir = sffs_readdir,
    .getattr = sffs_getattr,
    .mknod = sffs_mknod,
    .open = sffs_open,
    .read = sffs_read,
    .write = sffs_write,
    .unlink = sffs_unlink,
    .truncate = sffs_truncate,
    .statfs = sffs_statfs,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &op, NULL);
}
