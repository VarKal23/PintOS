#include "vm/page_table.h"
#include "vm/swap.h"
// why isn't it recognizing swap_in and swap_out
#include "vm/swap.c"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include <stdlib.h>

static bool my_install_page (void *upage, void *kpage, bool writable);

// comparator function for pages used by hash_init
bool page_addr_comparator (const struct hash_elem *a, 
            const struct hash_elem *b, void* aux UNUSED)
{
  const struct page_entry *page_a = hash_entry (a, struct page_entry, 
                                                hash_elem);
  const struct page_entry *page_b = hash_entry (b, struct page_entry, 
                                                hash_elem);
  return page_a->addr < page_b->addr;
}

// hash function for page table used by hash_init
unsigned page_table_hash_func (const struct hash_elem *e, void *aux) {
    const struct page_entry *page = hash_entry (e, struct page_entry, 
                                                hash_elem);
    return hash_int((int) page->addr);
}

// returns the page_entry associated with a virtual address
struct page_entry* get_page (void *vaddr) {
    if (!is_user_vaddr (vaddr)) return NULL;
    struct page_entry tmp;
    tmp.addr = pg_round_down (vaddr);
    struct hash_elem *elem = hash_find (thread_current ()->page_table, 
                                        &tmp.hash_elem);
    if (elem == NULL) return NULL;
    return hash_entry (elem, struct page_entry, hash_elem);
}

// allocates a new page for a given virtual address
struct page_entry* allocate_page (void *vaddr) {
    struct page_entry* p = malloc (sizeof(struct page_entry));
    if (p == NULL) return NULL;
    p->addr = pg_round_down (vaddr);
    if (get_page (vaddr)) return NULL;
    p->writable = false;
    p->swap_sector = -1;
    p->offset = 0;
    p->read_bytes = 0;
    p->zero_bytes= 0;
    block_sector_t swap_sector;
    p->owner = thread_current ();
    hash_insert(thread_current ()->page_table, &p->hash_elem);
    return p;
}

// TODO: page lock and unlock

// function that allocates page to stack & loads it into memory
bool grow_stack (void *vaddr) {
    int stack_size = (uintptr_t) PHYS_BASE - (uintptr_t) pg_round_down(vaddr);
    if ( stack_size > STACK_MAX) return false;
    struct page_entry* page = allocate_page(vaddr);
    if (!page) return false;
    page->writable = true;
    page->frame = allocate_frame();
    if (!page->frame) 
    {
        free(page);
        return false;
    }
    bool success = my_install_page(page->addr, page->frame->kvaddr, 
                                    page->writable);
    if (!success)
    {
      free_frame(page->frame);
      free(page);
      return false;
    }
    return true;
}

// function that loads a page into physical memory (if a file or in swap)
bool load_page(struct page_entry *page) {
    if (page->frame) return true;
    page->frame = allocate_frame();
    if (!page->frame) 
    {
        free(page);
        return false;
    }
    bool success = my_install_page(page->addr, page->frame->kvaddr, 
                                    page->writable);
    if (!success)
    {
      free_frame(page->frame);
      free(page);
      return false;
    }

    if (page->file) {
        acquire_filesys_lock();
        size_t read_bytes = file_read_at (page->file, page->frame->kvaddr,
                                        page->read_bytes, page->offset);
        memset (page->frame->kvaddr + read_bytes, 0, page->zero_bytes);
        if (read_bytes != page->read_bytes) {
            release_filesys_lock();
            free_frame(page->frame);
            free(page);
            return false;
        }
    } else if (page->swap_sector != -1) {
        swap_in (page->swap_sector, page->frame->kvaddr);
        page->swap_sector = -1;
    }
    return true;
}

// function that unloads a page from physical memory and stores in 
// swap/file system depending on contents. helper method for evict
bool unload_page(struct page_entry *page) {
    uint32_t* pd = page->owner->pagedir;
    void* addr = page->addr;
    pagedir_clear_page(pd, addr);
    int dirty = pagedir_is_dirty(pd, addr);
    bool success = true;
    if (!page->file) {
        if (swap_out(page->frame->kvaddr)) {
            page->frame = NULL;
            return true;
        }
        return false;
    }
    // page has a file
    if (dirty) {
        if(file_write_at(page->file, page->frame->kvaddr, 
                        page->read_bytes, page->offset)) {
            page->frame = NULL;
            return true;
        }
        return false;
    }
    page->frame = NULL;
    return true;
}

// function for checking if user is trying to allocate memory to stack
bool is_accessing_stack(void* esp, void *vaddr) {
    int stack_size = (uintptr_t) PHYS_BASE - (uintptr_t) pg_round_down(vaddr);
    // our heuristic is if vaddr is within 32 bytes of esp
    return stack_size < STACK_MAX && (uintptr_t) esp - 32 < vaddr;
}

// our version of install_page that adds an additional check
static bool my_install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL &&
          pagedir_set_page (t->pagedir, upage, kpage, writable));
}

// deletes a single page of memory, used in hash_destroy
static void destroy_page (struct hash_elem *e, void *aux UNUSED)
{
  struct page_entry *page = hash_entry(e, struct page_entry, 
                                        hash_elem);
  if (page->frame)
    {
      free_frame(page->frame);
      uint32_t* pd = page->owner->pagedir;
      void* addr = page->addr;
      pagedir_clear_page(pd, addr);
    }
  free(page);
  page = NULL;
}

// destroys entire page table. used in process_exit
void destroy_page_table (struct hash* page_table)
{
  hash_destroy (page_table, destroy_page);
}
