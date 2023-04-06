#ifndef SWAP_H
#define SWAP_H

#include "devices/block.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include <stdbool.h>

// Varun drove here

// block struct representing the swap partition
struct block *swap_block;
// bitmap used to track in-use and free swap slots
struct bitmap *swap_bitmap;
// lock that protects swap
struct lock swap_lock;

// initializes the swap partition, used in init.c
void init_swap (void);
// loads data from swap into a specified frame
bool swap_in (struct page_entry* page);
// moves data from frame to swap, used in eviction
bool swap_out (struct page_entry* page);

#endif // SWAP_H