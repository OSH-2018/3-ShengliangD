#ifndef PARAMS_H
#define PARAMS_H

#define FS_SIZE (4 * 1024 * 1024 * 1024ULL)

#define BLOCK_SIZE (4 * 1024)
#define BLOCK_NUM (FS_SIZE / BLOCK_SIZE)

#define BMBLOCK_BEGIN 1
#define BMBLOCK_NUM (BLOCK_NUM / 8 / BLOCK_SIZE)

#define IBLOCK_BEGIN (1 + BM_BLOCK_NUM)
#define IBLOCK_NUM 2

#endif
