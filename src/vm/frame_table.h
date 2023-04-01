#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/page_table.h"
// #include "threads/loader.h"
// #include "threads/palloc.h"

// our frame struct
struct frame_entry {
    struct page_entry* page;
    // TODO: should this void*?
    uint8_t* kvaddr; // think of it as physical memory
    struct thread* owner;
    // TODO: add a lock here
};

void init_frame_table();
struct frame_entry* allocate_frame();
void free_frame (struct frame_entry *frame);

#endif // FRAME_TABLE_H
