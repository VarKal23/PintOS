#include "vm/frame_table.h"
#include "vm/page_table.c"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "threads/loader.h"
#include "threads/vaddr.h"
#include <stdlib.h>

static struct frame_entry* frame_table;
struct lock frame_lock;
static size_t num_user_pages;
static size_t frame_cnt;

static struct frame_entry* evict_page();
static struct frame_entry* find_free_frame();

// initalizes frame table
void init_frame_table(void) {
    void *base;
    num_user_pages = get_num_user_pages();
    frame_table = malloc(num_user_pages * sizeof *frame_table);
    lock_init(&frame_lock);
    for (int i = 0; i < num_user_pages; i++) {
        struct frame_entry* cur_frame = &frame_table[i];
        // void* vaddr = palloc_get_page(PAL_USER);
        // if (vaddr == NULL) break;
        cur_frame->kvaddr = NULL;
        cur_frame->page = NULL;
        cur_frame->owner = NULL;
        // TODO: init individual locks
    }
}

// allocates a new frame, returns a frame_entry struct
struct frame_entry* allocate_frame() {
    // TODO: this might throw an error
    // lock_acquire(&frame_lock);
    struct frame_entry* free_frame = find_free_frame();
    // no pages available, use eviction policy
    if (free_frame == NULL) {
        // may still return null
        free_frame = evict_page();
    }
    // lock_release(&frame_lock);
    if (free_frame) {
        free_frame->kvaddr = palloc_get_page(PAL_USER);
    }
    return free_frame;
}

// searches frame table for empty frame and returns it
struct frame_entry* find_free_frame() {
    for (int i = 0; i < num_user_pages; i++) {
        struct frame_entry* cur_frame = &frame_table[i];
        if (cur_frame->page == NULL) {
            return cur_frame;
        }
    }
    return NULL;
}

// TODO: release a lock here
// frees a frame and removes any references of it
void free_frame (struct frame_entry *frame) {
    frame->page = NULL;
    palloc_free_page(frame->kvaddr);
}

// finds suitable page to evict from a given frame and returns pointer to frame
struct frame_entry* evict_page() {
    lock_acquire(&frame_lock);
    int index = 0;
    int num_trips = 0;
    bool notFound = true;

    while(notFound && num_trips < 2) {
        struct frame_entry* cur_frame = &frame_table[index];
        uint32_t* pd = cur_frame->owner->pagedir;
        void* addr = cur_frame->page->addr;
        int referenced = pagedir_is_accessed(pd, addr);
        // int dirty = pagedir_is_dirty(pd, addr);

        // not referenced, not dirty - found frame for eviction
        if (!referenced) {
            if (unload_page(cur_frame->page)) {
                notFound = false;
                cur_frame->owner = NULL;
                cur_frame->page = NULL;
                lock_release(&frame_lock);
                return cur_frame;
            }
        // referenced, not dirty
        } else if (referenced) {
            pagedir_set_accessed(pd, addr, false);
        }
        // referenced, dirty
        // } else if (dirty) {
        //     pagedir_set_dirty(pd, addr, false);
        // }

        // update index
        if (index + 1 == num_user_pages) {
            index = 0;
            num_trips++;
        } else {
            index++;
        }

    }
    lock_release(&frame_lock);
    return NULL;
}