#include "devices/block.h"
#include <bitmap.h>
#include "threads/vaddr.h"

struct block *swap_block;
struct bitmap *swap_bitmap;
struct lock swap_lock;

void swap_init (void);
size_t swap_out (void *frame);
void swap_in (size_t used_index, void* frame);