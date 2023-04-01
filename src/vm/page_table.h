#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#define STACK_MAX (512 * 1024)

#include "devices/block.h"
#include "filesys/off_t.h"
#include "vm/frame_table.h"
#include <hash.h>

// our page struct
// Matt & Varun drove here
struct page_entry {
    void *addr;
    struct thread* owner;
    // if frame is null, it is not in memory
    struct frame_entry* frame;
    struct hash_elem hash_elem;
    bool writable;

    struct file* file;
    off_t offset;
    size_t read_bytes;
    size_t zero_bytes;

    block_sector_t swap_sector;
};

hash_less_func page_addr_comparator;
hash_hash_func page_table_hash_func;

struct page_entry* get_page (void *vaddr);
struct page_entry* allocate_page (void *vaddr);
void remove_page (void *vaddr);
bool grow_stack (void *vaddr);
bool load_page(struct page_entry *page);
bool unload_page(struct page_entry *page);
bool is_accessing_stack(void* esp, void *vaddr);
void destroy_page_table (struct hash* page_table);

#endif // PAGE_TABLE_H