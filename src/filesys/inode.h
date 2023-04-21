#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include <list.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_BLOCKS 123
#define INDIRECT_BLOCKS 1
#define DOUBLE_INDIRECT_BLOCKS 1
#define MAX_ENTRIES_PER_BLOCK 128
#define MAX_INDIRECT_INDEX 252
#define MAX_DOUBLE_INDIRECT_INDEX 16384

struct bitmap;

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;    
    // Varun Driving                 /* Magic number. */
    block_sector_t direct_map[DIRECT_BLOCKS];
    block_sector_t indirect_block;
    block_sector_t doubly_indirect_block;
    bool is_dir;
  };
  
/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;            /* Inode content. */
    struct lock lock;
    off_t safe_length;
  };


void inode_init (void);
bool inode_create (block_sector_t, off_t, bool is_dir);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

#endif /* filesys/inode.h */
