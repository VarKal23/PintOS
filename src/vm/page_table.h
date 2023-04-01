#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#define STACK_MAX (512 * 1024)

#include "devices/block.h"
#include "filesys/off_t.h"
#include "vm/frame_table.h"
#include <hash.h>

// our page struct
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

// TODO: why doesn't this work?
hash_less_func page_addr_comparator;
hash_hash_func page_table_hash_func;

// bool page_addr_comparator(const struct hash_elem *a, const struct hash_elem *b, void *aux);
// unsigned page_table_hash_func(const struct hash_elem *e, void *aux);

struct page_entry* get_page (void *vaddr);
struct page_entry* allocate_page (void *vaddr);
void remove_page (void *vaddr);
bool grow_stack (void *vaddr);
bool load_page(struct page_entry *page);
bool unload_page(struct page_entry *page);
bool is_accessing_stack(void* esp, void *vaddr);
void page_table_destroy (struct hash* page_table);

#endif // PAGE_TABLE_H