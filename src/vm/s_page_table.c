#include "vm/s_page_table.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

bool page_addr_comparator (const struct hash_elem *a, 
            const struct hash_elem *b, void* aux UNUSED)
{
  const struct page_entry *page_a = hash_entry (a, struct page_entry, hash_elem);
  const struct page_entry *page_b = hash_entry (b, struct page_entry, hash_elem);
  return page_a->addr < page_b->addr;
}

// TODO: include thread->current as param?
struct page_entry* get_page (const void *vaddr) {
    if (!is_user_vaddr (vaddr)) return NULL;
    struct page_entry tmp;
    // TODO: round down in parent function?
    tmp.addr = pg_round_down (vaddr);
    // TODO: add page table to thread struct
    struct hash_elem *elem = hash_find (thread_current ()->page_table, &tmp.hash_elem);
    if (elem == NULL) return NULL;
    return hash_entry (elem, struct page_entry, hash_elem);
}

void remove_page (const void *vaddr) {
    struct page_entry *p = get_page (vaddr);
    // TODO: check if get_page returns null
    // TODO: evict from memory
    hash_delete (thread_current ()->page_table, &p->hash_elem);
    free (p);
}

// TODO: keep read_only as a param?
struct page_entry* insert_page (const void *vaddr, const bool read_only) {
    struct page_entry *p = malloc (sizeof(struct page_entry));
    if (p == NULL) return NULL;
    p->addr = pg_round_down (vaddr);
    if (get_page (vaddr)) return NULL;
    p->read_only = read_only;
    p->swap_sector = -1;
    p->offset = 0;
    p->owner = thread_current ();
    hash_insert(thread_current ()->page_table, &p->hash_elem);
    return p;
}

unsigned page_table_hash_func (const struct hash_elem *e, void *aux) {
    const struct page_entry *page = hash_entry (e, struct page_entry, hash_elem);
    return hash_int((int) page->addr);
}


// TODO: page hash fuction
// TODO: evict/load to/from memory
// page lock and unlock?
