#ifndef SWAP_H
#define SWAP_H

#include "devices/block.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include <stdbool.h>

// Varun drove here

struct block *swap_block;
struct bitmap *swap_bitmap;
struct lock swap_lock;

void init_swap (void);
bool swap_in (block_sector_t sector, void *frame);
bool swap_out (void *frame);

#endif // SWAP_H