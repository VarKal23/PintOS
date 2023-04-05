#include "vm/swap.h"
#include <stdlib.h>
#include <stdbool.h>

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

// initializes swap partition and bitmap for checking if they're occupied
// Varun drove here
void init_swap() {
    swap_block = block_get_role (BLOCK_SWAP);
    // block size returns number of sectors, but you want the bitmap 
    // to be the number of pages
    swap_bitmap = bitmap_create (block_size (swap_block) / SECTORS_PER_PAGE);
    lock_init(&swap_lock);
}

// loads data from swap into specified frame
// Matt drove here
bool swap_in (struct page_entry* page) {
    lock_acquire(&swap_lock);
    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
         block_read (swap_block, page->swap_sector + i,
                (uintptr_t) page->frame + i * BLOCK_SECTOR_SIZE);
    }
    // TODO: for some reason, I'm giving up my page->frame->lock here? why??
    // ASSERT(lock_held_by_current_thread(&page->frame->lock));
    bitmap_reset (swap_bitmap, page->swap_sector / SECTORS_PER_PAGE);
    lock_release(&swap_lock);
    page->swap_sector = -1;
    return true;
}

// moves data from frame to swap
// Matt drove here
bool swap_out (struct page_entry* page) {
    lock_acquire(&swap_lock);
    // find available sectors
    size_t free_slot = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
    // swap partition is full
    if (free_slot == BITMAP_ERROR) {
        lock_release(&swap_lock);
        return false;
    }
    block_sector_t starting_sector = free_slot * SECTORS_PER_PAGE;
    for (int i = 0; i < SECTORS_PER_PAGE; i++)
    {
      block_write(swap_block, starting_sector + i, 
                (uintptr_t) page->frame->kvaddr + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&swap_lock);
    page->swap_sector = starting_sector;
    page->frame = NULL;
    return true;
}

