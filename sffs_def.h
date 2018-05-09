#ifndef SFFS_DEF_H
#define SFFS_DEF_H

#include <time.h>

typedef unsigned long long ull;

#define FS_SIZE (4 * 1024 * 1024 * 1024ULL)

#define BLOCK_SIZE (4 * 1024)
#define BLOCK_NUM (FS_SIZE / BLOCK_SIZE)

#define BMBLOCK_BEGIN 1
#define BMBLOCK_NUM (BLOCK_NUM / 8 / BLOCK_SIZE)

#define IBLOCK_BEGIN (1 + BMBLOCK_NUM)
#define IBLOCK_NUM 2
#define INDEX_PER_BLOCK (BLOCK_SIZE / sizeof(ull))

// Meta data of this entire file system, always the first block
typedef struct {
    ull total_blocks;
    ull used_blocks;
} super_block_t;

typedef struct {
    ull prev;
    ull next;
} chain_t;

#define FNAME_LIMIT 255

// Meta data of a file
typedef struct {
    chain_t chain;
    char name[FNAME_LIMIT+1];
    time_t atime;
    time_t mtime;
    ull size;
} attr_block_t;

#define CBLOCK_CAP ((BLOCK_SIZE - sizeof(chain_t)) / sizeof(ull))

typedef struct {
    chain_t chain;
    ull data_block_ids[CBLOCK_CAP];
} chain_block_t;

// NOTE: seek_tuple_t will always be one step before the position to operate
typedef struct {
    ull chain_block_id;
    ull chain_block_seek;
    ull data_block_seek;
} seek_tuple_t;

#endif
