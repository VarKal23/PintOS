#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

// maximum size of stack (8 MB)
#define STACK_MAX (8 * 1024 * 1024)

#include "devices/block.h"
#include "filesys/off_t.h"
#include "vm/frame_table.h"
#include <hash.h>

// our page struct
// Matt & Varun drove here
struct page_entry {
    // virtual address where page starts
    void *addr;
    // the thread which the page belongs to
    struct thread* owner;
    // the frame associated with this page, if applicable
    // if frame is null, it is not in memory
    struct frame_entry* frame;
    // the element in the hash table that corresponds to this
    struct hash_elem hash_elem;
    // whether the page can be written to
    bool writable;

    // pointer to the file that this page’s data belongs to, if applicable
    struct file* file;
    // offset in file where the data begins
    off_t offset;
    // number of bytes from the offset that are stored
    size_t read_bytes;
    // number of zero bytes stored (PGSIZE - read_bytes)
    size_t zero_bytes;
    // sector in swap where this page’s data is stored, if applicable
    block_sector_t swap_sector;
};

// comparator function for page table used by hash_init
hash_less_func page_addr_comparator;
// hash function for page table used by hash_init
hash_hash_func page_table_hash_func;

// returns the page_entry associated with a virtual address
struct page_entry* get_page (void *vaddr);
// allocates a new page for a given virtual address
struct page_entry* allocate_page (void *vaddr);
// function that allocates page to stack & loads it into memory
bool grow_stack (void *vaddr);
// function that loads a page into physical memory (if a file or in swap)
bool load_page(struct page_entry *page);
// function that unloads a page from physical memory and stores it in 
// the swap/file system. helper method for evict
bool unload_page(struct page_entry *page);
// function for checking if user is trying to allocate memory to stack
bool is_accessing_stack(void* esp, void *vaddr);
// destroys an entire page table. used in process_exit
void destroy_page_table (struct hash* page_table);
// locks the frame associated with a page, loads the page into physical memory
// if it does not have a frame. Used in syscall.c when accessing user memory
bool lock_page(struct page_entry* page);
// unlocks the frame associated with a page, used in syscall.c
void unlock_page(struct page_entry* page);

#endif // PAGE_TABLE_H