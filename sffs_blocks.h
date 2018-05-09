#ifndef SFFS_BLOCKS_H
#define SFFS_BLOCKS_H

extern void * blocks[];

void mark_block(ull bid);
void unmark_block(ull bid);
void * new_block();
void delete_block(ull bid);
ull find_free_block();
ull alloc_block();
void free_block(ull bid);
void free_space(seek_tuple_t st);
int find_attr_block(const char *path, ull *iid, ull *bid, attr_block_t ** ab);
void next_chain_block(seek_tuple_t *st, int alloc);
void next_data_block(seek_tuple_t *st, int alloc);
void next_byte(seek_tuple_t *st, int alloc);
void locate(off_t offset, const attr_block_t *ab, seek_tuple_t *st);

#endif