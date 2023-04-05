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
static size_t est_num_frames;
static size_t num_frames;

static struct frame_entry* evict_page();
static struct frame_entry* find_free_frame();

// Varun drove here
// initalizes frame table
void init_frame_table(void) {
    est_num_frames = get_num_user_pages();
    frame_table = malloc(est_num_frames * sizeof(struct frame_entry));
    lock_init(&frame_lock);
    while (true) {
        struct frame_entry* cur_frame = &frame_table[num_frames];
        cur_frame->kvaddr = palloc_get_page(PAL_USER);
        if (!cur_frame->kvaddr) {
            break;
        }
        num_frames++;
        cur_frame->page = NULL;
        cur_frame->owner = NULL;
        lock_init(&cur_frame->lock);
    }
}

// allocates a new frame, returns a frame_entry struct
// acquires the frame's lock for pinning
// Varun drove here
struct frame_entry* allocate_frame(struct page_entry* page) {
    lock_acquire(&frame_lock);
    struct frame_entry* free_frame = find_free_frame();
    // no pages available, use eviction policy
    if (free_frame == NULL) {
        // may still return null
        free_frame = evict_page();
    }
    lock_release(&frame_lock);
    if (free_frame) {
        free_frame->owner = page->owner;
        free_frame->page = page;
    }
    return free_frame;
}

// searches frame table for empty frame and returns it
// Matt drove here
struct frame_entry* find_free_frame() {
    for (int i = 0; i < num_frames; i++) {
        struct frame_entry* cur_frame = &frame_table[i];
        if (lock_held_by_current_thread (&cur_frame->lock) || !lock_try_acquire (&cur_frame->lock)) {
            continue;
        }
        if (cur_frame->page == NULL) {
            return cur_frame;
        }
        lock_release (&cur_frame->lock);
    }
    return NULL;
}

// finds suitable page to evict from a given frame and returns pointer to frame
// Varun & Matt drove here
struct frame_entry* evict_page() {
    int index = 0;
    int num_trips = 0;

    while(num_trips < 2) {
        struct frame_entry* cur_frame = &frame_table[index];
        if (lock_held_by_current_thread (&cur_frame->lock) || !lock_try_acquire (&cur_frame->lock)) {
            continue;
        }
        uint32_t* pd = cur_frame->owner->pagedir;
        void* addr = cur_frame->page->addr;
        int referenced = pagedir_is_accessed(pd, addr);
        if (!referenced) {
            if (unload_page(cur_frame->page)) {
                free_frame(cur_frame);
                // because free frame will release the lock
                lock_acquire(&cur_frame->lock);
                return cur_frame;
            }
        } else {
            pagedir_set_accessed(pd, addr, false);
        }
        lock_release (&cur_frame->lock);
        // update index
        if (index + 1 == num_frames) {
            index = 0;
            num_trips++;
        } else {
            index++;
        }
    }
    return NULL;
}

// frees a frame and removes any references of it
// Varun drove here
void free_frame (struct frame_entry *frame) {
    if (!lock_held_by_current_thread (&frame->lock)) {
        lock_acquire(&frame->lock);
    }
    frame->page = NULL;
    frame->owner = NULL;
    lock_release (&frame->lock);
}