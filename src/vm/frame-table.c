#include "vm/frame-table.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include <stdlib.h>
#include "threads/loader.h"

static struct frame_entry** frame_table;
struct lock frame_lock;
static size_t num_user_pages;

static struct frame_entry* evict_page();
static struct frame_entry* find_free_frame();

void init_frame_table() {
    num_user_pages = get_num_user_pages();
    frame_table = malloc(num_user_pages * sizeof(struct frame_entry*));
    lock_init(&frame_lock);
    for (int i = 0; i < num_user_pages; i++) {
        struct frame_entry* cur_frame = malloc(sizeof(struct frame_entry));
        cur_frame->kvaddr = NULL;
        cur_frame->page_in_frame = NULL;
        cur_frame->owner = NULL;
        frame_table[i] = cur_frame;
    }
}

// Returns kernel vaddr of available frame
uint8_t* get_frame(uint8_t* upage) {
    
    struct frame_entry* free_frame = find_free_frame();

    // no pages available, use eviction policy
    if (free_frame == NULL) {
        free_frame = evict_page();
    }
    // Rearrange this into initialize? Wasn't working when i did that
    if (free_frame->kvaddr == NULL) {
        free_frame->kvaddr = palloc_get_page(PAL_USER);
    }
    free_frame->owner = thread_current();
    // Make SPE for upage?
    struct page_entry* new_page = malloc(sizeof(struct page_entry));
    new_page->upage = upage;
    free_frame->page_in_frame = new_page;

    return free_frame->kvaddr;
}

// Search frame table for empty frame and return it
struct frame_entry* find_free_frame() {
    for (int i = 0; i < num_user_pages; i++) {
        struct frame_entry* cur_frame = frame_table[i];
        if (cur_frame->page_in_frame == NULL) {
            return cur_frame;
        }
    }
    return NULL;
}

// Finds suitable page to evict from a given frame and returns pointer to frame
struct frame_entry* evict_page() {
    int index = 0;
    bool notFound = true;

    // second chance page replacement algorithm; HOW do i keep clock hand set at one place and not reset everytime? Global pointer to current frame_entry?
    while(notFound) {
        struct frame_entry* cur_frame = frame_table[index];
        uint32_t* pd = cur_frame->owner->pagedir;
        void* upage = cur_frame->page_in_frame->upage;
        int referenced = pagedir_is_accessed(pd, upage);
        int dirty = pagedir_is_dirty(pd, upage);

        // not referenced, not dirty - found frame for eviction
        if (!referenced && !dirty) {
            notFound = false;
            // What's this mean?
            // fe->loaded_page->loaded = false;
            // palloc_free_page(upage);
            // void* kvaddr = palloc_get_page(PAL_USER);
            cur_frame->owner = NULL;
            cur_frame->page_in_frame = NULL;
            return cur_frame;

        // referenced, not dirty
        } else if (referenced) {
            pagedir_set_accessed(pd, upage, false);

        // referenced, dirty
        } else if (dirty) {
            // acquire_file_lock();
            // // file_seek();
            // // file_write();
            // release_file_lock();
            pagedir_set_dirty(pd, upage, false);
        }
        index++;

    }
}



