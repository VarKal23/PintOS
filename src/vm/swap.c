#include "vm/swap.h"

void swap_init() {
    swap_block = block_get_role (BLOCK_SWAP);
    if (!swap_block) PANIC ("Failed to created swap block");
    // block size returns number of sectors, but you want the bitmap to be the number of pages
    swap_bitmap = bitmap_create (block_size (swap_block) / (PGSIZE / BLOCK_SECTOR_SIZE));
    if (!swap_bitmap) PANIC ("Failed to created swap bitmap");
    lock_init(&swap_lock);
}

void swap_in() {
    
}

void swap_out() {
    lock_acquire(&swap_lock);
    size_t free_slot = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
    lock_release(&swap_lock);
    if (!free_slot) PANIC ("Swap partition is full");
    return free_slot;
}

