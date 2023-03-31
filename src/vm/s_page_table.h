// TODO: just make these ints?
#include "devices/block.h"
#include "filesys/off_t.h"
#include <hash.h>
// include more stuff?

struct page_entry {
    // same as upage
    void *addr;
    struct hash_elem hash_elem;
    block_sector_t sector;
    struct file* file;
    off_t offset;
    bool read_only;
    struct thread* owner;
    // number of bytes to read or write?
    // have a pointer to frame?
};