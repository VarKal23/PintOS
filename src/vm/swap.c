#include "vm/swap.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_init() {
    swap_block = block_get_role (BLOCK_SWAP);
    if (!swap_block) PANIC ("Failed to created swap block");
    // block size returns number of sectors, but you want the bitmap to be the number of pages
    swap_bitmap = bitmap_create (block_size (swap_block) / SECTORS_PER_PAGE);
    if (!swap_bitmap) PANIC ("Failed to created swap bitmap");
    lock_init(&swap_lock);
}

void swap_in (block_sector_t sector, void *frame) {
    // TODO: check to make sure frame and sector are valid
    lock_acquire(&swap_lock);
    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
         block_read (swap_block, sector + i,
                frame + i * BLOCK_SECTOR_SIZE);
    }
    bitmap_reset (swap_bitmap, sector / SECTORS_PER_PAGE);
    lock_release(&swap_lock);
}

block_sector_t swap_out (void *frame) {
    lock_acquire(&swap_lock);
    // TODO: right type?
    block_sector_t sector = bitmap_scan_and_flip(swap_bitmap, 0, 1, false) * SECTORS_PER_PAGE;
    if (!sector) PANIC ("Swap partition is full");
    for (int i = 0; i < SECTORS_PER_PAGE; i++)
    {
    //   TODO: should i cast?
      block_write(swap_block, sector + i, frame + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&swap_lock);
    return sector;
}

