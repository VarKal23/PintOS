#include "vm/swap.h"
#include <stdlib.h>
#include <stdbool.h>

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

// initializes swap partition and bitmap for checking if they're occupied
void init_swap() {
    swap_block = block_get_role (BLOCK_SWAP);
    if (!swap_block) PANIC ("Failed to created swap block");
    // block size returns number of sectors, but you want the bitmap to be the number of pages
    swap_bitmap = bitmap_create (block_size (swap_block) / SECTORS_PER_PAGE);
    if (!swap_bitmap) PANIC ("Failed to created swap bitmap");
    lock_init(&swap_lock);
}

// loads data from swap into specified frame
bool swap_in (block_sector_t sector, void *frame) {
    // TODO: check to make sure frame and sector are valid
    lock_acquire(&swap_lock);
    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
         block_read (swap_block, sector + i,
                (uintptr_t) frame + i * BLOCK_SECTOR_SIZE);
    }
    bitmap_reset (swap_bitmap, sector / SECTORS_PER_PAGE);
    lock_release(&swap_lock);
    return true;
}

// moves data from frame to swap
bool swap_out (void *frame) {
    lock_acquire(&swap_lock);
    // find available sectors
    block_sector_t sector = bitmap_scan_and_flip(swap_bitmap, 0, 1, false) * SECTORS_PER_PAGE;
    // swap partition is full
    if (!sector) return false;
    for (int i = 0; i < SECTORS_PER_PAGE; i++)
    {
    //   TODO: should i cast?
      block_write(swap_block, sector + i, (uintptr_t) frame + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&swap_lock);
    return true;
}

