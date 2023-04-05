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
// Matt drove here
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
// Matt drove here
unsigned page_table_hash_func (const struct hash_elem *e, void *aux) {
    const struct page_entry *page = hash_entry (e, struct page_entry, 
                                                hash_elem);
    return hash_int((int) page->addr);
}

// returns the page_entry associated with a virtual address
// Matt drove here
struct page_entry* get_page (void *vaddr) {
    if (!is_user_vaddr (vaddr)) return NULL;
    struct page_entry tmp;
    tmp.addr = pg_round_down (vaddr);
    struct hash_elem *elem = hash_find (&thread_current ()->page_table, 
                                        &tmp.hash_elem);
    if (!elem) return NULL;
    return hash_entry (elem, struct page_entry, hash_elem);
}

// allocates a new page for a given virtual address
// Varun drove here
struct page_entry* allocate_page (void *vaddr) {
    struct page_entry* p = malloc (sizeof(struct page_entry));
    if (!p) return NULL;
    p->addr = pg_round_down (vaddr);
    if (get_page (vaddr)) return NULL;
    p->frame = NULL;
    p->file = NULL;
    p->offset = 0;
    p->read_bytes = 0;
    p->zero_bytes= 0;
    p->writable = false;
    p->swap_sector = -1;
    p->owner = thread_current ();
    hash_insert(&thread_current ()->page_table, &p->hash_elem);
    return p;
}

// TODO: page lock and unlock

// function that allocates page to stack & loads it into memory
// Matt drove here
bool grow_stack (void *vaddr) {
    // printf("entered grow_stack\n");
    int stack_size = (uintptr_t) PHYS_BASE - (uintptr_t) pg_round_down(vaddr);
    if ( stack_size > STACK_MAX) return false;
    struct page_entry* page = allocate_page(vaddr);
    if (!page) return false;
    page->writable = true;
    page->frame = allocate_frame(page);
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
    lock_release(&page->frame->lock);
    return true;
}

// function that loads a page into physical memory (if a file or in swap)
// Matt drove here
bool load_page(struct page_entry *page) {
    // the frame's lock will be acquired in allocate_frame
    page->frame = allocate_frame(page);
    if (!page->frame)
    {
        free(page);
        return false;
    }
    // adds page to the real page table
    bool success = my_install_page(page->addr, page->frame->kvaddr, 
                                    page->writable);
    if (!success)
    {
      free_frame(page->frame);
      free(page);
      return false;
    }
    // Varun driving now
    if (page->read_bytes == 0) {
        memset (page->frame->kvaddr, 0, page->zero_bytes);
    } else if (page->file) {
        acquire_filesys_lock();
        size_t read_bytes = file_read_at (page->file, page->frame->kvaddr,
                                        page->read_bytes, page->offset);
        memset (page->frame->kvaddr + read_bytes, 0, page->zero_bytes);
        release_filesys_lock();
        if (read_bytes != page->read_bytes) {
            free_frame(page->frame);
            free(page);
            return false;
        }
    } else if (page->swap_sector != -1) {
        swap_in (page);
    }
    lock_release(&page->frame->lock);
    // printf("page successfully allocated\n");
    return true;
}

// function that unloads a page from physical memory and stores in 
// swap/file system. helper method for evict
// Matt drove here
bool unload_page(struct page_entry *page) {
    uint32_t* pd = page->owner->pagedir;
    void* addr = page->addr;
    pagedir_clear_page(pd, addr);
    int dirty = pagedir_is_dirty(pd, addr);
    if (!page->file) {
        if (!swap_out(page)) {
            return false;
        }
    }
    // page has a file
    else if (dirty) {
        size_t bytes_written = file_write_at(page->file, page->frame->kvaddr, 
                    page->read_bytes, page->offset);
        if (bytes_written != page->read_bytes) {
            // TODO: the problem is here
            ASSERT(false);
            // return false;
        }
    }
    page->frame = NULL;
    return true;
}

// function for checking if user is trying to allocate memory to stack
// Varun drove here
bool is_accessing_stack(void* esp, void *vaddr) {
    int stack_size = (uintptr_t) PHYS_BASE - (uintptr_t) pg_round_down(vaddr);
    // our heuristic is if vaddr is within 32 bytes of esp
    return stack_size < STACK_MAX && (uintptr_t) esp - 32 <= vaddr;
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

// Varun drove here
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
      free(page->frame);
      page->frame = NULL;
    }
  free(page);
}

// destroys entire page table. used in process_exit
void destroy_page_table (struct hash* page_table)
{
    hash_destroy (page_table, destroy_page);
}

// used in syscall
bool lock_page(struct page_entry* page) {
    if (page->frame) {
        if (!lock_held_by_current_thread(&page->frame->lock)) {
            lock_acquire(&page->frame->lock);
        }
        return true;
    } else {
        return load_page(page);
    }
}

void unlock_page(struct page_entry* page) {
    if (lock_held_by_current_thread(&page->frame->lock)) {
        lock_release(&page->frame->lock);
    }
}

