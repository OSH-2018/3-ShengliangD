#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>

#include "sffs_def.h"

void * blocks[BLOCK_NUM];

void set_bitmap_ull(ull bid, ull bits) {
    bid = bid / (8*sizeof(ull));    
    ull num_64_per_block = BLOCK_SIZE / sizeof(ull);
    ((ull*)blocks[BMBLOCK_BEGIN + bid / num_64_per_block])[bid % num_64_per_block] = bits;
}

ull get_bitmap_ull(ull bid) {
    bid = bid / (8*sizeof(ull));
    ull num_64_per_block = BLOCK_SIZE / sizeof(ull);
    return ((ull*)blocks[BMBLOCK_BEGIN + bid / num_64_per_block])[bid % num_64_per_block];
}

// Find the first 0 bit
#define bits_find_0(bits) (__builtin_ctzll(~bits))

void * new_block() {
    void * ret;
    assert ( (ret = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) != MAP_FAILED );
    memset(ret, 0, BLOCK_SIZE);
    return ret;
}

void delete_block(ull bid) {
    assert ( munmap(blocks[bid], BLOCK_SIZE) != -1 );
}

ull find_free_block() {
        ull bid;
        ull bitmap_64;
        for (bid = 0; (bitmap_64 = get_bitmap_ull(bid)) == 0xffffffffffffffff; bid += 64)
            ;
        bid += bits_find_0(bitmap_64);
        return bid;
}

void mark_block(ull bid) {
    // TODO: check if used
    ull bits = get_bitmap_ull(bid);
    assert ( (bits & (1ULL << (bid % 64))) == 0 );
    bits |= (1ULL << (bid % 64));
    set_bitmap_ull(bid, bits);
}

void unmark_block(ull bid) {
    ull bits = get_bitmap_ull(bid);
    assert ( (bits | ~(1ULL << (bid % 64))) == 0xffffffffffffffff );    
    bits &= ~(1ULL << (bid % 64));
    set_bitmap_ull(bid, bits);
}

ull alloc_block() {
    ull bid = find_free_block();

    // TODO: judge if there is no free block

    blocks[bid] = new_block();
    mark_block(bid);

    ++(((super_block_t*)(blocks[0]))->used_blocks);

    return bid;
}

void free_block(ull bid) {
    delete_block(bid);
    unmark_block(bid);
    blocks[bid] = 0;
    --(((super_block_t*)(blocks[0]))->used_blocks);    
}

void free_space(seek_tuple_t st) {
    // Fill the remaining of the current data block with 0, then goto next data block
    {
        chain_block_t * cb = (chain_block_t*)blocks[st.chain_block_id];
        memset(
            (char*)blocks[cb->data_block_ids[st.chain_block_seek]] + st.data_block_seek + 1,
            0,
            BLOCK_SIZE - st.data_block_seek - 1
        );
        st.data_block_seek = BLOCK_SIZE - 1;
    }

    // Free all the data blocks in current chain block, then goto next chain block
    {
        chain_block_t * cb = (chain_block_t*)blocks[st.chain_block_id];        
        while (st.chain_block_seek + 1 < CBLOCK_CAP && cb->data_block_ids[st.chain_block_seek + 1] != 0) {
            free_block(cb->data_block_ids[st.chain_block_seek + 1]);
            ++st.chain_block_seek;
        }
    }

    // Then free all that remains
    {
        chain_block_t * cb = (chain_block_t*)blocks[st.chain_block_id];
        while (cb->chain.next != 0) {
            st.chain_block_id = cb->chain.next;
            chain_block_t * tcb = (chain_block_t*)blocks[cb->chain.next];
            cb->chain.next = tcb->chain.next;

            st.chain_block_seek = 0;
            while (st.chain_block_seek < CBLOCK_CAP && tcb->data_block_ids[st.chain_block_seek] != 0) {
                free_block(tcb->data_block_ids[st.chain_block_seek]);
                ++st.chain_block_seek;
            }
            free_block(st.chain_block_id);
        }
    }
}

int find_attr_block(const char *path, ull *iid, ull *bid, attr_block_t ** ab) {
    for (ull i = 0; i < IBLOCK_NUM; ++i) {
        for (ull j = 0; j < BLOCK_SIZE / sizeof (ull); ++j) {
            ull tbid = ((ull*)blocks[IBLOCK_BEGIN + i])[j];
            attr_block_t * attr = (attr_block_t *)blocks[tbid];
            if (tbid != 0 && (strcmp(path+1, attr->name) == 0)) {
                if (iid != NULL)
                    *iid = i * (BLOCK_SIZE / sizeof(ull)) + j;
                if (bid != NULL)
                    *bid = tbid;
                if (ab != NULL)
                    *ab = attr;
                return 0;
            }
        }
    }
    return -ENOENT;
}

void next_chain_block(seek_tuple_t *st, int alloc) {
    chain_block_t * cb = (chain_block_t*)blocks[st->chain_block_id];    

    if (alloc && cb->chain.next == 0)
        cb->chain.next = alloc_block();

    st->chain_block_id = cb->chain.next;
}

void next_data_block(seek_tuple_t *st, int alloc) {
    if (st->chain_block_seek + 1 == CBLOCK_CAP) {
        next_chain_block(st, alloc);
        st->chain_block_seek = 0;
    } else {
        ++st->chain_block_seek;
    }

    chain_block_t * cb = (chain_block_t*)blocks[st->chain_block_id];        
    if (alloc && cb->data_block_ids[st->chain_block_seek] == 0)
        cb->data_block_ids[st->chain_block_seek] = alloc_block();
}

void next_byte(seek_tuple_t *st, int alloc) {
    if (st->data_block_seek + 1 == BLOCK_SIZE) {
        next_data_block(st, alloc);
        st->data_block_seek = 0;
    } else {
        ++st->data_block_seek;
    }
}

// Locate the cb and bseek to offset.
// The resulting position is actually offset-1, because
// we need to allocate new space if there isn't.
void locate(off_t offset, const attr_block_t *ab, seek_tuple_t *st) {
    st->chain_block_id = ab->chain.prev;
    st->chain_block_seek = CBLOCK_CAP - 1;
    st->data_block_seek = BLOCK_SIZE - 1;

    off_t ofst = 0;

    while (ofst < ab->size && ofst < offset && offset - ofst >= CBLOCK_CAP * BLOCK_SIZE) {
        next_chain_block(st, 0);
        ofst += CBLOCK_CAP * BLOCK_SIZE;
    }

    while (ofst < ab->size && ofst < offset && offset - ofst >= BLOCK_SIZE) {
        next_data_block(st, 0);
        ofst += BLOCK_SIZE;
    }

    while (ofst < ab->size && ofst < offset) {
        next_byte(st, 0);
        ofst += 1;
    }
}
