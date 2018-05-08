#ifndef PARAMS_H
#define PARAMS_H

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

typedef struct {
    ull prev;
    ull next;
} block_comm_t;

#define BLOCK_CAP (BLOCK_SIZE - sizeof(block_comm_t))

#define FNAME_LIMIT 1024

typedef enum {
    FT_FILE,  
} ftype_t;

typedef struct {
    ull total_blocks;
    ull used_blocks;
} super_block_t;

typedef struct {
    block_comm_t comm;
    ftype_t type;
    time_t atime;
    time_t mtime;    
    ull size;
    char name[FNAME_LIMIT];
    ull last_block;
} attr_block_t;

typedef struct {
    block_comm_t comm;
    unsigned char data[BLOCK_CAP];
} comm_block_t;

#endif
