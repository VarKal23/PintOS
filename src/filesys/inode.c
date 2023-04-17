// #include "filesys/inode.h"
// #include <list.h>
// #include <debug.h>
// #include <round.h>
// #include <string.h>
// #include "filesys/filesys.h"
// #include "filesys/free-map.h"
// #include "threads/malloc.h"

// /* Identifies an inode. */
// #define INODE_MAGIC 0x494e4f44

// #define DIRECT_BLOCKS 124
// #define INDIRECT_BLOCKS 1
// #define DOUBLE_INDIRECT_BLOCKS 1
// #define MAX_ENTRIES_PER_BLOCK 128
// #define MAX_INDIRECT_INDEX 252
// #define MAX_DOUBLE_INDIRECT_INDEX 16636

// static bool allocate_direct_blocks(struct inode* inode, block_sector_t direct_sectors);
// static bool allocate_indirect_blocks(struct inode* inode, block_sector_t indirect_sectors);
// static bool allocate_double_indirect_blocks(struct inode* inode, block_sector_t double_indirect_sectors);
// /* On-disk inode.
//    Must be exactly BLOCK_SECTOR_SIZE bytes long. */
// struct inode_disk
// {
//   // block_sector_t start; /* First data sector. */
//   off_t length;         /* File size in bytes. */
//   unsigned magic;       /* Magic number. */
//   block_sector_t direct_map[124];
//   block_sector_t indirect_block;
//   block_sector_t doubly_indirect_block;
// };

// /* Returns the number of sectors to allocate for an inode SIZE
//    bytes long. */
// static inline size_t bytes_to_sectors (off_t size)
// {
//   return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
// }

// /* In-memory inode. */
// struct inode
// {
//   struct list_elem elem;  /* Element in inode list. */
//   block_sector_t sector;  /* Sector number of disk location. */
//   int open_cnt;           /* Number of openers. */
//   bool removed;           /* True if deleted, false otherwise. */
//   int deny_write_cnt;     /* 0: writes ok, >0: deny writes. */
//   struct inode_disk data; /* Inode content. */

//   off_t length;
//   block_sector_t direct_map[124];
//   block_sector_t indirect_block;
//   block_sector_t doubly_indirect_block;
// };

// /* Returns the block device sector that contains byte offset POS
//    within INODE.
//    Returns -1 if INODE does not contain data for a byte at offset
//    POS. */
// static block_sector_t byte_to_sector (const struct inode *inode, off_t pos)
// {
//   ASSERT (inode != NULL);
//   int block_index = pos / BLOCK_SECTOR_SIZE;

//   block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];

//   // Direct mapped block access
//   if (block_index < DIRECT_BLOCKS) {
//     return inode->data.direct_map[block_index];

//   // indirect block access
//   } else if (block_index < MAX_INDIRECT_INDEX) {
//     block_index -= DIRECT_BLOCKS;
//     block_read(ffs_device, s_device, inode->data.indirect_block, &indirect_buffer);
//     return indirect_buffer[block_index];

//   } else if (block_index < MAX_DOUBLE_INDIRECT_INDEX) {
//     int indirect_block_index = (block_index - MAX_INDIRECT_INDEX) / MAX_ENTRIES_PER_BLOCK;
//     block_read(ffs_device, s_device, inode->data.doubly_indirect_block, &indirect_buffer);
//     block_sector_t indirect_block = indirect_buffer[indirect_block_index];

//     int direct_block_index = (block_index - MAX_INDIRECT_INDEX) % MAX_ENTRIES_PER_BLOCK;
//     block_read(ffs_device, s_device, indirect_block, &indirect_buffer);
//     return indirect_buffer[direct_block_index];
//   } else
//     return -1;
// }

// /* List of open inodes, so that opening a single inode twice
//    returns the same `struct inode'. */
// static struct list open_inodes;

// /* Initializes the inode module. */
// void inode_init (void) { list_init (&open_inodes); }

// /* Initializes an inode with LENGTH bytes of data and
//    writes the new inode to sector SECTOR on the file system
//    device.
//    Returns true if successful.
//    Returns false if memory or disk allocation fails. */
// bool inode_create(block_sector_t sector, off_t length)
// {
//     struct inode_disk *disk_inode = NULL;
//     bool success = false;

//     ASSERT(length >= 0);

//     ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

//     disk_inode = calloc(1, sizeof *disk_inode);
//     if (disk_inode != NULL)
//     {
//         block_sector_t sectors = bytes_to_sectors(length);
//         disk_inode->length = length;
//         disk_inode->magic = INODE_MAGIC;

//         block_sector_t direct_sectors = sectors > DIRECT_BLOCKS ? DIRECT_BLOCKS : sectors;
//         block_sector_t indirect_sectors;
//         block_sector_t double_indirect_sectors;
//         if (sectors <= DIRECT_BLOCKS) {
//           direct_sectors = sectors;
//           indirect_sectors = 0;
//           double_indirect_sectors = 0;
//           indirect_sectors = sectors - DIRECT_BLOCKS;
//         } else if (sectors <= MAX_INDIRECT_INDEX) {
//           direct_sectors = DIRECT_BLOCKS;
//           indirect_sectors = sectors - DIRECT_BLOCKS;
//           double_indirect_sectors = 0;
//         } else {
//           direct_sectors = DIRECT_BLOCKS;
//           indirect_sectors = MAX_INDIRECT_INDEX - DIRECT_BLOCKS;
//           double_indirect_sectors = sectors - MAX_INDIRECT_INDEX;
//         }
//         struct inode* inode = calloc(0, sizeof(struct inode));
//         inode->length = length;
//         ASSERT(direct_sectors != 1);
//         if (allocate_direct_blocks(inode, direct_sectors) &&
//             allocate_indirect_blocks(inode, indirect_sectors) &&
//             allocate_double_indirect_blocks(disk_inode, double_indirect_sectors))
//         {
//             block_write(ffs_device, s_device, sector, disk_inode);
//             success = true;
//         }
//         free(disk_inode);
//     }
//     return success;
// }

// static bool allocate_direct_blocks(struct inode *inode, block_sector_t direct_sectors) {
//     for (int i = 0; i < direct_sectors; i++) {
//         if (!free_map_allocate(1, &inode->direct_map[i]))
//             return false;
//     }
//     return true;
// }

// static bool allocate_indirect_blocks(struct inode* inode, block_sector_t indirect_sectors) {
//     if (indirect_sectors == 0) {
//       return true;
//     }

//     if (!free_map_allocate(1, &inode->indirect_block)) {
//       return false;
//     }

//     block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];
//     memset(&indirect_buffer, 0, sizeof(indirect_buffer));

//     for (size_t i = 0; i < indirect_sectors; i++) {
//       if (!free_map_allocate(1, &indirect_buffer[i])) {
//         return false;
//       }
//     }

//     block_write(ffs_device, s_device, inode->indirect_block, &indirect_buffer);
//     return true;  
// }

// static bool allocate_double_indirect_blocks(struct inode *inode, block_sector_t double_indirect_sectors) {
//     if (double_indirect_sectors == 0) {
//       return true;
//     }

//     if (!free_map_allocate(1, &inode->doubly_indirect_block)) {
//       return false;
//     }

//     block_sector_t double_indirect_buffer[MAX_ENTRIES_PER_BLOCK];  
//     block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];

//     memset(&double_indirect_buffer, 0, sizeof(double_indirect_buffer));

//     for (int i = 0; i < double_indirect_sectors; i++) {
//         if (!free_map_allocate(1, &double_indirect_buffer[i]))
//             return false;
//         // TODO: if on last block, only allocate the remaining sectors
//         for (int j = 0; j < MAX_ENTRIES_PER_BLOCK; j++) {
//           if (!free_map_allocate(1, &indirect_buffer[j])) {
//             return false;
//           }

//         }
//         block_write(ffs_device, s_device, double_indirect_buffer[i], &indirect_buffer);
//     }
//     block_write(ffs_device, s_device, inode->doubly_indirect_block, &double_indirect_buffer);
//     return true;
// }

// /* Reads an inode from SECTOR
//    and returns a `struct inode' that contains it.
//    Returns a null pointer if memory allocation fails. */
// struct inode *inode_open (block_sector_t sector)
// {
//   struct list_elem *e;
//   struct inode *inode;

//   /* Check whether this inode is already open. */
//   for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
//        e = list_next (e))
//     {
//       inode = list_entry (e, struct inode, elem);
//       if (inode->sector == sector)
//         {
//           inode_reopen (inode);
//           return inode;
//         }
//     }

//   /* Allocate memory. */
//   inode = malloc (sizeof *inode);
//   if (inode == NULL)
//     return NULL;

//   /* Initialize. */
//   list_push_front (&open_inodes, &inode->elem);
//   inode->sector = sector;
//   inode->open_cnt = 1;
//   inode->deny_write_cnt = 0;
//   inode->removed = false;
//   block_read (fs_device, fs_device, inode->sector, &inode->data);
//   return inode;
// }

// /* Reopens and returns INODE. */
// struct inode *inode_reopen (struct inode *inode)
// {
//   if (inode != NULL)
//     inode->open_cnt++;
//   return inode;
// }

// /* Returns INODE's inode number. */
// block_sector_t inode_get_inumber (const struct inode *inode)
// {
//   return inode->sector;
// }

// /* Closes INODE and writes it to disk. (Does it?  Check code.)
//    If this was the last reference to INODE, frees its memory.
//    If INODE was also a removed inode, frees its blocks. */
// void inode_close (struct inode *inode)
//   {
//   /* Ignore null pointer. */
//   if (inode == NULL)
//     return;

//   /* Release resources if this was the last opener. */
//   if (--inode->open_cnt == 0) {
//     if (inode->removed) {
//       /* Remove from inode list and release lock. */
//       list_remove (&inode->elem);

//       /* Deallocate blocks if removed. */
//       free_map_release (inode->sector, 1);
//       size_t sectors = bytes_to_sectors(inode->length);
//       size_t direct_sectors = sectors > DIRECT_BLOCKS ? DIRECT_BLOCKS : sectors;
//       size_t indirect_sectors;
//       size_t double_indirect_sectors;
//       if (sectors <= DIRECT_BLOCKS) {
//         direct_sectors = sectors;
//         indirect_sectors = 0;
//         double_indirect_sectors = 0;
//       } else if (sectors <= MAX_INDIRECT_INDEX) {
//         direct_sectors = DIRECT_BLOCKS;
//         indirect_sectors = sectors - DIRECT_BLOCKS;
//         double_indirect_sectors = 0;
//       } else {
//         direct_sectors = DIRECT_BLOCKS;
//         indirect_sectors = MAX_INDIRECT_INDEX - DIRECT_BLOCKS;
//         double_indirect_sectors = sectors - MAX_INDIRECT_INDEX;
//       }
//       for (size_t i = 0; i < direct_sectors; i++) {
//         free_map_release(inode->direct_map[i], 1);
//       }
//       if (indirect_sectors > 0) {
//         block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];
//         block_read(ffs_device, s_device, inode->indirect_block, &indirect_buffer);
//         for (size_t i = 0; i < indirect_sectors; i++) {
//           free_map_release(indirect_buffer[i], 1);
//         }
//         free_map_release(inode->indirect_block, 1);
//       }
//       if (double_indirect_sectors > 0) {
//         block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];
//         block_sector_t double_indirect_buffer[MAX_ENTRIES_PER_BLOCK];
//         block_read(ffs_device, s_device, inode->doubly_indirect_block, &double_indirect_buffer);
//         for (size_t i = 0; i < MAX_ENTRIES_PER_BLOCK; i++) {
//           if (double_indirect_buffer[i] == 0) {
//             break;
//           }
//           block_read(ffs_device, s_device, double_indirect_buffer[i], &indirect_buffer);
//           for (size_t j = 0; j < MAX_ENTRIES_PER_BLOCK; j++) {
//             if (indirect_buffer[j] == 0) {
//               break;
//             }
//             free_map_release(indirect_buffer[j], 1);
//           }
//           free_map_release(double_indirect_buffer[i], 1);
//         }
//         free_map_release(inode->doubly_indirect_block, 1);
//       }

//       free (inode);
//     } else {
//       block_write(ffs_device, s_device, inode->sector, &inode->data);
//       free(inode);
//     }
//   }
// }

// /* Marks INODE to be deleted when it is closed by the last caller who
//    has it open. */
// void inode_remove (struct inode *inode)
// {
//   ASSERT (inode != NULL);
//   inode->removed = true;
// }

// /* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
//    Returns the number of bytes actually read, which may be less
//    than SIZE if an error occurs or end of file is reached. */
// off_t inode_read_at (struct inode *inode, void *buffer_, off_t size,
//                      off_t offset)
// {
//   uint8_t *buffer = buffer_;
//   off_t bytes_read = 0;
//   uint8_t *bounce = NULL;

//   while (size > 0)
//     {
//       /* Disk sector to read, starting byte offset within sector. */
//       block_sector_t sector_idx = byte_to_sector (inode, offset);
//       int sector_ofs = offset % BLOCK_SECTOR_SIZE;

//       /* Bytes left in inode, bytes left in sector, lesser of the two. */
//       off_t inode_left = inode_length (inode) - offset;
//       int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
//       int min_left = inode_left < sector_left ? inode_left : sector_left;

//       /* Number of bytes to actually copy out of this sector. */
//       int chunk_size = size < min_left ? size : min_left;
//       if (chunk_size <= 0)
//         break;

//       if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
//         {
//           /* Read full sector directly into caller's buffer. */
//           block_read (fs_device, fs_device, sector_idx, buffer + bytes_read);
//         }
//       else
//         {
//           /* Read sector into bounce buffer, then partially copy
//              into caller's buffer. */
//           if (bounce == NULL)
//             {
//               bounce = malloc (BLOCK_SECTOR_SIZE);
//               if (bounce == NULL)
//                 break;
//             }
//           block_read (fs_device, fs_device, sector_idx, bounce);
//           memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
//         }

//       /* Advance. */
//       size -= chunk_size;
//       offset += chunk_size;
//       bytes_read += chunk_size;
//     }
//   free (bounce);

//   return bytes_read;
// }

// /* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
//    Returns the number of bytes actually written, which may be
//    less than SIZE if end of file is reached or an error occurs.
//    (Normally a write at end of file would extend the inode, but
//    growth is not yet implemented.) */
// off_t inode_write_at (struct inode *inode, const void *buffer_, off_t size,
//                       off_t offset)
// {
//   const uint8_t *buffer = buffer_;
//   off_t bytes_written = 0;
//   uint8_t *bounce = NULL;

//   if (inode->deny_write_cnt)
//     return 0;

//   while (size > 0)
//     {
//       /* Sector to write, starting byte offset within sector. */
//       block_sector_t sector_idx = byte_to_sector (inode, offset);
//       int sector_ofs = offset % BLOCK_SECTOR_SIZE;

//       /* Bytes left in inode, bytes left in sector, lesser of the two. */
//       off_t inode_left = inode_length (inode) - offset;
//       int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
//       int min_left = inode_left < sector_left ? inode_left : sector_left;

//       /* Number of bytes to actually write into this sector. */
//       int chunk_size = size < min_left ? size : min_left;
//       if (chunk_size <= 0)
//         break;

//       if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
//         {
//           /* Write full sector directly to disk. */
//           block_write (fs_device, fs_device, sector_idx, buffer + bytes_written);
//         }
//       else
//         {
//           /* We need a bounce buffer. */
//           if (bounce == NULL)
//             {
//               bounce = malloc (BLOCK_SECTOR_SIZE);
//               if (bounce == NULL)
//                 break;
//             }

//           /* If the sector contains data before or after the chunk
//              we're writing, then we need to read in the sector
//              first.  Otherwise we start with a sector of all zeros. */
//           if (sector_ofs > 0 || chunk_size < sector_left)
//             block_read (fs_device, fs_device, sector_idx, bounce);
//           else
//             memset (bounce, 0, BLOCK_SECTOR_SIZE);
//           memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
//           block_write (fs_device, fs_device, sector_idx, bounce);
//         }

//       /* Advance. */
//       size -= chunk_size;
//       offset += chunk_size;
//       bytes_written += chunk_size;
//     }
//   free (bounce);

//   return bytes_written;
// }

// /* Disables writes to INODE.
//    May be called at most once per inode opener. */
// void inode_deny_write (struct inode *inode)
// {
//   inode->deny_write_cnt++;
//   ASSERT (inode->deny_write_cnt <= inode->open_cnt);
// }

// /* Re-enables writes to INODE.
//    Must be called once by each inode opener who has called
//    inode_deny_write() on the inode, before closing the inode. */
// void inode_allow_write (struct inode *inode)
// {
//   ASSERT (inode->deny_write_cnt > 0);
//   ASSERT (inode->deny_write_cnt <= inode->open_cnt);
//   inode->deny_write_cnt--;
// }

// /* Returns the length, in bytes, of INODE's data. */
// off_t inode_length (const struct inode *inode) { return inode->data.length; }

#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_BLOCKS 124
#define INDIRECT_BLOCKS 1
#define DOUBLE_INDIRECT_BLOCKS 1
#define MAX_ENTRIES_PER_BLOCK 128
#define MAX_INDIRECT_INDEX 252
#define MAX_DOUBLE_INDIRECT_INDEX 16384

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    block_sector_t direct_map[DIRECT_BLOCKS];
    block_sector_t indirect_block;
    block_sector_t doubly_indirect_block;
  };

struct inode_indirect_block_sector {
  block_sector_t blocks[MAX_ENTRIES_PER_BLOCK];
};

static bool inode_allocate (struct inode_disk *disk_inode);
static bool inode_reserve (struct inode_disk *disk_inode, off_t length);
static bool inode_deallocate (struct inode *inode);
static bool allocate_direct_blocks(struct inode_disk* inode, block_sector_t direct_sectors);
static bool allocate_indirect_blocks(struct inode_disk* inode, block_sector_t indirect_sectors);
static bool allocate_double_indirect_blocks(struct inode_disk* inode, block_sector_t double_indirect_sectors);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

static inline size_t
min (size_t a, size_t b)
{
  return a < b ? a : b;
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  if (0 <= pos && pos < inode->data.length) {
    off_t block_index = pos / BLOCK_SECTOR_SIZE;

    block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];

    // Direct mapped block access
    if (block_index < DIRECT_BLOCKS) {
      return inode->data.direct_map[block_index];

    //indirect block access
    } else if (block_index < MAX_INDIRECT_INDEX) {
      block_index -= DIRECT_BLOCKS;
      block_read(fs_device, inode->data.indirect_block, &indirect_buffer);
      block_sector_t found_block = indirect_buffer[block_index];
      return indirect_buffer[block_index];

    } else if (block_index < MAX_DOUBLE_INDIRECT_INDEX) {
      int indirect_block_index = (block_index - MAX_INDIRECT_INDEX) / MAX_ENTRIES_PER_BLOCK;

      // Reads first level doubly indirect block
      block_read(fs_device, inode->data.doubly_indirect_block, &indirect_buffer);
      block_sector_t indirect_block = indirect_buffer[indirect_block_index];

      int direct_block_index = (block_index - MAX_INDIRECT_INDEX) % MAX_ENTRIES_PER_BLOCK;

      // Reads the 2nd level indirect block containing our desired block
      block_read(fs_device, indirect_block, &indirect_buffer);
      return indirect_buffer[direct_block_index];
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
// bool
// inode_create (block_sector_t sector, off_t length)
// {
//   struct inode_disk *disk_inode = NULL;
//   bool success = false;

//   ASSERT (length >= 0);

//   /* If this assertion fails, the inode structure is not exactly
//      one sector in size, and you should fix that. */
//   ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

//   disk_inode = calloc (1, sizeof *disk_inode);
//   if (disk_inode != NULL)
//     {
//       disk_inode->length = length;
//       disk_inode->magic = INODE_MAGIC;
//       if (inode_allocate (disk_inode))
//         {
//           block_write (fs_device, sector, disk_inode);
//           success = true;
//         }
//       free (disk_inode);
//     }
//   return success;
// }

bool inode_create(block_sector_t sector, off_t length)
{
    struct inode_disk *disk_inode = NULL;
    bool success = false;

    ASSERT(length >= 0);

    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL)
    {
        block_sector_t sectors = bytes_to_sectors(length);
        disk_inode->length = length;
        disk_inode->magic = INODE_MAGIC;

        block_sector_t direct_sectors = sectors > DIRECT_BLOCKS ? DIRECT_BLOCKS : sectors;
        block_sector_t indirect_sectors;
        block_sector_t double_indirect_sectors;
        if (sectors <= DIRECT_BLOCKS) {
          direct_sectors = sectors;
          indirect_sectors = 0;
          double_indirect_sectors = 0;
        } else if (sectors <= MAX_INDIRECT_INDEX) {
          direct_sectors = DIRECT_BLOCKS;
          indirect_sectors = sectors - DIRECT_BLOCKS;
          double_indirect_sectors = 0;
        } else {
          direct_sectors = DIRECT_BLOCKS;
          indirect_sectors = MAX_INDIRECT_INDEX - DIRECT_BLOCKS;
          double_indirect_sectors = sectors - MAX_INDIRECT_INDEX;
        }
        disk_inode->length = length;
        if (allocate_direct_blocks(disk_inode, direct_sectors) &&
            allocate_indirect_blocks(disk_inode, indirect_sectors) &&
            allocate_double_indirect_blocks(disk_inode, double_indirect_sectors))
        {
            block_write(fs_device, sector, disk_inode);
            success = true;
        }
        free(disk_inode);
    }
    return success;
}

static bool allocate_direct_blocks(struct inode_disk *disk_inode, block_sector_t direct_sectors) {
    for (int i = 0; i < direct_sectors; i++) {
        if (!free_map_allocate(1, &disk_inode->direct_map[i]))
            return false;
    }
    return true;
}

static bool allocate_indirect_blocks(struct inode_disk* disk_inode, block_sector_t indirect_sectors) {
    if (indirect_sectors == 0) {
      return true;
    }

    if (!free_map_allocate(1, &disk_inode->indirect_block)) {
      return false;
    }

    block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];
    memset(&indirect_buffer, 0, sizeof(indirect_buffer));

    for (size_t i = 0; i < indirect_sectors; i++) {
      if (!free_map_allocate(1, &indirect_buffer[i])) {
        return false;
      }
    }
    block_write(fs_device, disk_inode->indirect_block, &indirect_buffer);
    return true;  
}

static bool allocate_double_indirect_blocks(struct inode_disk *disk_inode, block_sector_t double_indirect_sectors) {
    if (double_indirect_sectors == 0) {
      return true;
    }

    if (!free_map_allocate(1, &disk_inode->doubly_indirect_block)) {
      return false;
    }

    block_sector_t double_indirect_buffer[MAX_ENTRIES_PER_BLOCK];  
    block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];

    memset(&double_indirect_buffer, 0, sizeof(double_indirect_buffer));

    for (int i = 0; i < double_indirect_sectors; i++) {
        if (!free_map_allocate(1, &double_indirect_buffer[i]))
            return false;
        // TODO: if on last block, only allocate the remaining sectors
        for (int j = 0; j < MAX_ENTRIES_PER_BLOCK; j++) {
          if (!free_map_allocate(1, &indirect_buffer[j])) {
            return false;
          }

        }
        block_write(fs_device, double_indirect_buffer[i], &indirect_buffer);
    }
    block_write(fs_device, disk_inode->doubly_indirect_block, &double_indirect_buffer);
    return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          free_map_release (inode->sector, 1);
          inode_deallocate (inode);
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode).
   */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  // beyond the EOF: extend the file
  if( byte_to_sector(inode, offset + size - 1) == -1u ) {
    // extend and reserve up to [offset + size] bytes
    bool success;
    success = inode_reserve (& inode->data, offset + size);
    if (!success) return 0;  // fail?

    // write back the (extended) file size
    inode->data.length = offset + size;
    block_write (fs_device, inode->sector, & inode->data);
  }

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else
        {
          /* We need a bounce buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}


static
bool inode_allocate (struct inode_disk *disk_inode)
{
  return inode_reserve (disk_inode, disk_inode->length);
}

static void
inode_reserve_indirect (block_sector_t* p_entry, size_t num_sectors, int level)
{
  static char zeros[BLOCK_SECTOR_SIZE];

  // only supports 2-level indirect block scheme as of now
  ASSERT (level <= 2);

  if (level == 0) {
    // base case : allocate a single sector if necessary and put it into the block
    if (*p_entry == 0) {
      free_map_allocate (1, p_entry);
      block_write (fs_device, *p_entry, zeros);
    }
    return;
  }

  struct inode_indirect_block_sector indirect_block;
  if(*p_entry == 0) {
    // not yet allocated: allocate it, and fill with zero
    free_map_allocate (1, p_entry);
    block_write (fs_device, *p_entry, zeros);
  }
  block_read(fs_device, *p_entry, &indirect_block);

  size_t unit = (level == 1 ? 1 : MAX_ENTRIES_PER_BLOCK);
  size_t i, l = DIV_ROUND_UP (num_sectors, unit);

  for (i = 0; i < l; ++ i) {
    size_t subsize = min(num_sectors, unit);
    inode_reserve_indirect (& indirect_block.blocks[i], subsize, level - 1);
    num_sectors -= subsize;
  }

  ASSERT (num_sectors == 0);
  block_write (fs_device, *p_entry, &indirect_block);
}

/**
 * Extend inode blocks, so that the file can hold at least
 * `length` bytes.
 */
static bool
inode_reserve (struct inode_disk *disk_inode, off_t length)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  if (length < 0) return false;

  // (remaining) number of sectors, occupied by this file.
  size_t num_sectors = bytes_to_sectors(length);
  size_t i, l;

  // (1) direct blocks
  l = min(num_sectors, DIRECT_BLOCKS * 1);
  for (i = 0; i < l; ++ i) {
    if (disk_inode->direct_map[i] == 0) { // unoccupied
      free_map_allocate (1, &disk_inode->direct_map[i]);
      block_write (fs_device, disk_inode->direct_map[i], zeros);
    }
  }
  num_sectors -= l;
  if(num_sectors == 0) return true;

  // (2) a single indirect block
  l = min(num_sectors, 1 * MAX_ENTRIES_PER_BLOCK);
  inode_reserve_indirect (& disk_inode->indirect_block, l, 1);
  num_sectors -= l;
  if(num_sectors == 0) return true;

  // (3) a single doubly indirect block
  l = min(num_sectors, 1 * MAX_ENTRIES_PER_BLOCK * MAX_ENTRIES_PER_BLOCK);
  inode_reserve_indirect (& disk_inode->doubly_indirect_block, l, 2);
  num_sectors -= l;
  if(num_sectors == 0) return true;

  ASSERT (num_sectors == 0);
  return false;
}

static void
inode_deallocate_indirect (block_sector_t entry, size_t num_sectors, int level)
{
  // only supports 2-level indirect block scheme as of now
  ASSERT (level <= 2);

  if (level == 0) {
    free_map_release (entry, 1);
    return;
  }

  struct inode_indirect_block_sector indirect_block;
  block_read(fs_device, entry, &indirect_block);

  size_t unit = (level == 1 ? 1 : MAX_ENTRIES_PER_BLOCK);
  size_t i, l = DIV_ROUND_UP (num_sectors, unit);

  for (i = 0; i < l; ++ i) {
    size_t subsize = min(num_sectors, unit);
    inode_deallocate_indirect (indirect_block.blocks[i], subsize, level - 1);
    num_sectors -= subsize;
  }

  ASSERT (num_sectors == 0);
  free_map_release (entry, 1);
}

static
bool inode_deallocate (struct inode *inode)
{
  off_t file_length = inode->data.length; // bytes
  if(file_length < 0) return false;

  // (remaining) number of sectors, occupied by this file.
  size_t num_sectors = bytes_to_sectors(file_length);
  size_t i, l;

  // (1) direct blocks
  l = min(num_sectors, DIRECT_BLOCKS * 1);
  for (i = 0; i < l; ++ i) {
    free_map_release (inode->data.direct_map[i], 1);
  }
  num_sectors -= l;

  // (2) a single indirect block
  l = min(num_sectors, 1 * MAX_ENTRIES_PER_BLOCK);
  if(l > 0) {
    inode_deallocate_indirect (inode->data.indirect_block, l, 1);
    num_sectors -= l;
  }

  // (3) a single doubly indirect block
  l = min(num_sectors, 1 * MAX_ENTRIES_PER_BLOCK * MAX_ENTRIES_PER_BLOCK);
  if(l > 0) {
    inode_deallocate_indirect (inode->data.doubly_indirect_block, l, 2);
    num_sectors -= l;
  }

  ASSERT (num_sectors == 0);
  return true;
}