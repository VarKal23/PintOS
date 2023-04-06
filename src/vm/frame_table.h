#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/page_table.h"

// Varun drove here
// our frame struct
struct frame_entry {
    // the page associated with this frame, if applicable
    struct page_entry* page;
    // the kernel virtual address where the frame starts
    uint8_t* kvaddr; // think of it as physical memory
    // the thread which the frame belongs to, if applicable
    struct thread* owner;
    // the pinning lock for this frame
    struct lock lock;
};

// initializes the frame table, used in init.c
void init_frame_table();
// function to allocate a new frame. In this function we search 
// for a free frame, and if one is not found, perform our eviction algorithm.
struct frame_entry* allocate_frame(struct page_entry* page);
// frees a frame and resets all its properties
void free_frame (struct frame_entry *frame);

#endif // FRAME_TABLE_H
