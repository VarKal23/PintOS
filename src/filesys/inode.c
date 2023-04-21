#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include <stdbool.h>

static bool inode_allocate (struct inode_disk *disk_inode);
static bool inode_reserve (struct inode_disk *disk_inode, off_t length);
static bool inode_deallocate (struct inode *inode);
static bool allocate_direct_blocks(struct inode_disk* inode, 
                                  block_sector_t direct_sectors);
static bool allocate_indirect_blocks(struct inode_disk* inode, 
                                  block_sector_t indirect_sectors);
static bool allocate_double_indirect_blocks(struct inode_disk* inode, 
                                  block_sector_t double_indirect_sectors);
static bool inode_grow(struct inode_disk* disk_inode, off_t length);

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

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  // Matthew Driving
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

    // Varun Driving
    } else if (block_index < MAX_DOUBLE_INDIRECT_INDEX) {
      int indirect_block_index = (block_index - MAX_INDIRECT_INDEX) / 
                                  MAX_ENTRIES_PER_BLOCK;

      // Reads first level doubly indirect block
      block_read(fs_device, inode->data.doubly_indirect_block, 
                &indirect_buffer);
      block_sector_t indirect_block = indirect_buffer[indirect_block_index];

      int direct_block_index = (block_index - MAX_INDIRECT_INDEX) % 
                                MAX_ENTRIES_PER_BLOCK;

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
// Varun Driving
bool inode_create(block_sector_t sector, off_t length, bool is_dir)
{
    struct inode_disk *disk_inode = NULL;

    ASSERT(length >= 0);

    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL)
    {
        if (!inode_grow(disk_inode, length)) {
          return false;
        }
        disk_inode->is_dir = is_dir;
        block_write(fs_device, sector, disk_inode);
        free(disk_inode);
        return true;
    }
    return false;
}

// Matthew Driving
/* Allocates inodes on disk_inode and grows past EOF if needed. */
static bool inode_grow(struct inode_disk* disk_inode, off_t length) {
  block_sector_t sectors = bytes_to_sectors(length);
  disk_inode->length = length;
  disk_inode->magic = INODE_MAGIC;

  block_sector_t direct_sectors = sectors > DIRECT_BLOCKS ? 
                                  DIRECT_BLOCKS : sectors;
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
  // Varun Driving
  } else {
    // comment
    direct_sectors = DIRECT_BLOCKS;
    indirect_sectors = MAX_INDIRECT_INDEX - DIRECT_BLOCKS;
    double_indirect_sectors = sectors - MAX_INDIRECT_INDEX;
  }
  disk_inode->length = length;
  if (allocate_direct_blocks(disk_inode, direct_sectors) &&
      allocate_indirect_blocks(disk_inode, indirect_sectors) &&
      allocate_double_indirect_blocks(disk_inode, double_indirect_sectors))
  {
      return true;
  }
  return false;
}

/*Allocate direct_sectors onto direct map blocks onto disk_inode*/
static bool allocate_direct_blocks(struct inode_disk *disk_inode, 
                                  block_sector_t direct_sectors) {
    for (int i = 0; i < direct_sectors; i++) {
      if (disk_inode->direct_map[i] == 0) {
        if (!free_map_allocate(1, &disk_inode->direct_map[i])) {
          return false;
        }
      }
    }
    return true;
}

/*Allocate indirect_sectors onto indirect block onto disk_inode*/
static bool allocate_indirect_blocks(struct inode_disk* disk_inode, 
                                    block_sector_t indirect_sectors) {
    if (indirect_sectors == 0) {
      return true;
    }

    if (disk_inode->indirect_block == 0) {
      if (!free_map_allocate(1, &disk_inode->indirect_block)) {
        return false;
      }
    }
    // Matthew Driving
    block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];
    // memset(&indirect_buffer, 0, sizeof(indirect_buffer));
    block_read(fs_device, disk_inode->indirect_block, &indirect_buffer);

    for (size_t i = 0; i < indirect_sectors; i++) {
      if (indirect_buffer[i] == 0) {
        if (!free_map_allocate(1, &indirect_buffer[i])) {
          return false;
        }
      }
    }

    block_write(fs_device, disk_inode->indirect_block, &indirect_buffer);
    return true;
}

// Varun Driving
/*Allocate double_indirect_sectors onto disk_inode*/
static bool allocate_double_indirect_blocks(struct inode_disk *disk_inode, 
                                      block_sector_t double_indirect_sectors) {
    if (double_indirect_sectors == 0) {
      return true;
    }

    if (disk_inode->doubly_indirect_block == 0) {
      if (!free_map_allocate(1, &disk_inode->doubly_indirect_block)) {
        return false;
      }
    }

    block_sector_t double_indirect_buffer[MAX_ENTRIES_PER_BLOCK];  
    block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];

    block_read(fs_device, disk_inode->doubly_indirect_block, 
              &double_indirect_buffer);
    // Matthew Driving
    for (int i = 0; i < double_indirect_sectors; i++) {
      if (double_indirect_buffer[i] == 0) { 
        if (!free_map_allocate(1, &double_indirect_buffer[i])) {
            return false;
        }
      }
      block_read(fs_device, double_indirect_buffer[i], &indirect_buffer);

      for (int j = 0; j < MAX_ENTRIES_PER_BLOCK; j++) {
        if (indirect_buffer[j] == 0) { 
          if (!free_map_allocate(1, &indirect_buffer[j])) {
            return false;
          }
        }
      }
      block_write(fs_device, double_indirect_buffer[i], &indirect_buffer);
    }
    block_write(fs_device, disk_inode->doubly_indirect_block, 
                &double_indirect_buffer);
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
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0) {
    /* Remove from inode list and release lock. */
    list_remove (&inode->elem);
    if (inode->removed) {
      /* Deallocate blocks if removed. */
      free_map_release (inode->sector, 1);

      // Matthew Driving
      size_t sectors = bytes_to_sectors(inode->data.length);
      size_t direct_sectors = sectors > DIRECT_BLOCKS ? DIRECT_BLOCKS : 
                              sectors;
      size_t indirect_sectors;
      size_t double_indirect_sectors;
      if (sectors <= DIRECT_BLOCKS) {
        direct_sectors = sectors;
        indirect_sectors = 0;
        double_indirect_sectors = 0;
      } else if (sectors <= MAX_INDIRECT_INDEX) {
        direct_sectors = DIRECT_BLOCKS;
        indirect_sectors = sectors - DIRECT_BLOCKS;
        double_indirect_sectors = 0;
      // Varun Driving
      } else {
        direct_sectors = DIRECT_BLOCKS;
        indirect_sectors = MAX_INDIRECT_INDEX - DIRECT_BLOCKS;
        double_indirect_sectors = sectors - MAX_INDIRECT_INDEX;
      }

      for (size_t i = 0; i < direct_sectors; i++) {
        free_map_release(inode->data.direct_map[i], 1);
      }
      if (indirect_sectors > 0) {
        block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];
        block_read(fs_device, inode->data.indirect_block, &indirect_buffer);
        for (size_t i = 0; i < indirect_sectors; i++) {
          free_map_release(indirect_buffer[i], 1);
        }
        free_map_release(inode->data.indirect_block, 1);
      }
      if (double_indirect_sectors > 0) {
        block_sector_t indirect_buffer[MAX_ENTRIES_PER_BLOCK];
        block_sector_t double_indirect_buffer[MAX_ENTRIES_PER_BLOCK];
        block_read(fs_device, inode->data.doubly_indirect_block, 
                  &double_indirect_buffer);
        for (size_t i = 0; i < MAX_ENTRIES_PER_BLOCK; i++) {
          if (double_indirect_buffer[i] == 0) {
            break;
          }
          // Matthew Driving
          block_read(fs_device, double_indirect_buffer[i], &indirect_buffer);
          for (size_t j = 0; j < MAX_ENTRIES_PER_BLOCK; j++) {
            if (indirect_buffer[j] == 0) {
              break;
            }
            free_map_release(indirect_buffer[j], 1);
          }
          free_map_release(double_indirect_buffer[i], 1);
        }
        free_map_release(inode->data.doubly_indirect_block, 1);
      }

      free (inode);
    } else {
      free(inode);
    }
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

  // Matthew Driving
  if(inode->data.length < offset + size) {
    if (!inode_grow(&inode->data, offset + size)) {
      return 0;
    }
    block_write (fs_device, inode->sector, &inode->data);
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