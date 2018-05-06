#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>
#include <time.h>

#define DEBUG(...)\
        {\
            fprintf(stderr,"%s:%d:%s(): ",__FILE__,__LINE__,__func__);\
            fprintf(stderr,__VA_ARGS__);\
            fprintf(stderr, "\n");\
        }

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "params.h"

void * blocks[BLOCK_NUM];

void set_bitmap_64(ull bid, ull bits) {
    bid = bid / 64;    
    ull num_64_per_block = BLOCK_SIZE / sizeof(ull);
    ((ull*)blocks[BMBLOCK_BEGIN + bid / num_64_per_block])[bid % num_64_per_block] = bits;
}

ull get_bitmap_of_block(ull bid) {
    bid = bid / 64;
    ull num_64_per_block = BLOCK_SIZE / sizeof(ull);
    return ((ull*)blocks[BMBLOCK_BEGIN + bid / num_64_per_block])[bid % num_64_per_block];
}

void * new_block() {
    void * ret;
    assert ( (ret = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) != MAP_FAILED );
    memset(ret, 0, BLOCK_SIZE);
    return ret;
}

void delete_block(void * ptr) {
    assert ( munmap(ptr, BLOCK_SIZE) != -1 );
}

// Find the first 0 bit
int bits_find_0(ull bits) {
    int ret = 0;
    // TODO: use asm
    while (bits % 2 == 1) {
        bits >>= 1;
        ++ret;
    }
    return ret;
}

ull find_free_block() {
        ull bid;
        ull bitmap_64;
        for (bid = 0; (bitmap_64 = get_bitmap_of_block(bid)) == 0xffffffffffffffff; bid += 64)
            ;
        bid += bits_find_0(bitmap_64);
        return bid;
}

void mark_block(ull bid) {
    // TODO: check if used
    ull bits = get_bitmap_of_block(bid);
    assert ( (bits & (1ULL << (bid % 64))) == 0 );
    bits |= (1ULL << (bid % 64));
    set_bitmap_64(bid, bits);
}

ull alloc_block() {
    ull bid = find_free_block();

    // TODO: judge if no free block

    blocks[bid] = new_block();
    mark_block(bid);

    return bid;
}

void free_block() {
}

ull find_attr_block(const char *path) {
    for (ull i = 0; i < IBLOCK_NUM; ++i) {
        for (ull j = 0; j < BLOCK_SIZE / sizeof (ull); ++j) {
            int bid = ((ull*)blocks[IBLOCK_BEGIN + i])[j];
            attr_block_t * attr = (attr_block_t *)blocks[bid];
            if (bid != 0 && (strcmp(path+1, attr->name) == 0)) {
                return bid;
            }
        }
    }
    return 0;
}

void unmark_block(ull bid) {
    // TODO: unset mark, set to NULL
}

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
                st.st_atime = st.st_mtime = st.st_ctime = attr->time;
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
        {
            ull bid = find_attr_block(path);
            if (bid == 0)
                return -ENOENT;
            ab = (attr_block_t*)blocks[bid];            
        }

        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFREG | 0755;
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = ab->time;
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

    // Fill file attr into the found block
    attr_block_t attr;
    attr.comm.prev = attr.comm.next = 0;
    strncpy(attr.name, path + 1, FNAME_LIMIT);
    attr.size = 0;
    time(&attr.time);
    attr.type = FT_FILE;
    memcpy(blocks[bid], &attr, sizeof(attr));

    return 0;
}

static int sffs_open(const char *path, struct fuse_file_info *fi) {
    return 0;
}

static int sffs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Find the attr block
    ull bid = find_attr_block(path);
    if (bid == 0)
        return -ENOENT;
    attr_block_t * ab = (attr_block_t*)blocks[bid];
    DEBUG("fname: %s", ab->name);

    // Locate the offset
    // TODO: offset ignored for now

    // From beginning
    comm_block_t * cb = (comm_block_t*)ab;
    ull bseek = BLOCK_CAP;
    // Read byte by byte
    size_t seek;
    for (seek = 0; seek < size; ++seek) {
        if (bseek == BLOCK_CAP) {
            if (cb->comm.next == 0) { // File end
                break;
            }
            cb = (comm_block_t*)blocks[cb->comm.next];
            bseek = 0;
        }
        buf[seek] = cb->data[bseek++];
    }
    DEBUG("fcontent: %s", buf);
    return seek;
}

static int sffs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Find the attr block
    ull bid = find_attr_block(path);
    if (bid == 0)
        return -ENOENT;
    attr_block_t * ab = (attr_block_t*)blocks[bid];

    // Locate the data block to write data
    // TODO: offset ignored for now

    // From beginning
    ab->size = size;
    comm_block_t * cb = (comm_block_t*)ab;
    ull bseek = BLOCK_CAP; // special value that specs this block end
    // Write byte by byte
    size_t seek;
    for (seek = 0; seek < size; ++seek) {
        if (bseek == BLOCK_CAP) {  // Current block is full, goto next block
            if (cb->comm.next == 0) {
                cb->comm.next = alloc_block();
                ((comm_block_t*)blocks[cb->comm.next])->comm.prev = bid;
                cb = (comm_block_t*)blocks[bid];
            }
            bid = cb->comm.next;                        
            cb = (comm_block_t*)blocks[bid];
            bseek = 0;
        }
        cb->data[bseek++] = buf[seek];
    }

    return seek;
}

static int sffs_unlink(const char *path)
{
    return 0;
}

static int sffs_truncate(const char *path, off_t size) {
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
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &op, NULL);
}
