#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/page_table.h"

// Varun drove here
// our frame struct
struct frame_entry {
    struct page_entry* page;
    uint8_t* kvaddr; // think of it as physical memory
    struct thread* owner;
    struct lock lock;
};

void init_frame_table();
struct frame_entry* allocate_frame(struct page_entry* page);
void free_frame (struct frame_entry *frame);

#endif // FRAME_TABLE_H
