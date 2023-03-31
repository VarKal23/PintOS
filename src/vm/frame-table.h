#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/s_page_table.h"
// #include "threads/loader.h"
// #include "threads/palloc.h"

// need user pool page number from palloc.c
// extern struct frame_entry* frame_table[];

struct frame_entry {
    struct page_entry* page_in_frame;
    uint8_t* kvaddr; // Think of it as physical memory
    struct thread* owner;
};

void init_frame_table();
uint8_t* get_frame(uint8_t* upage);
// struct frame_entry* find_empty_frame();
