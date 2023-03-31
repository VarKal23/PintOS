#include "devices/block.h"
#include <bitmap.h>
#include "threads/vaddr.h"

struct block *swap_block;
struct bitmap *swap_bitmap;
struct lock swap_lock;

void swap_init (void);
block_sector_t swap_out (void *frame);
void swap_in (block_sector_t sector, void *frame);