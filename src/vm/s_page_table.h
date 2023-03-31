// TODO: just make these ints?
#include "devices/block.h"
#include "filesys/off_t.h"
#include <hash.h>
// include more stuff?

struct page_entry {
    // same as upage
    void *upage;
    void *addr;
    struct hash_elem hash_elem;
    block_sector_t swap_sector;
    struct file* file;
    off_t offset;
    bool read_only;
    struct thread* owner;
    // number of bytes to read or write?
    // have a pointer to frame?
};

bool page_addr_comparator (const struct hash_elem *a, const struct hash_elem *b, void* aux UNUSED);
struct page_entry* get_page (const void *vaddr);
void remove_page (const void *vaddr);
struct page_entry* insert_page (const void *vaddr, const bool read_only);