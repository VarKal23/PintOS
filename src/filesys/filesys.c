#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void filesys_done (void) { free_map_close (); }

static struct dir *get_parent_directory (const char *path, char *file_name)
{
  struct dir *dir;
  char *token, *next_token, *save_ptr;
  // printf("%s", path);

  if (strlen(path) == 0) {
    return NULL;
  }

  if (path[0] == '/') {
    dir = dir_open_root ();
    path++;
  } else {
    if (!thread_current ()->cwd) {
      dir = dir_open_root ();
    }
    else {
      // dir = dir_open_root ();
      dir = dir_reopen (thread_current ()->cwd);
    }
  }

  char *path_cpy = malloc( (strlen(path) + 1));
  memcpy (path_cpy, path, (strlen(path) + 1));

  token = strtok_r (path_cpy, "/", &save_ptr);

  while (token != NULL)
  {
    next_token = strtok_r (NULL, "/", &save_ptr);
    if (next_token == NULL)
      break;

    struct inode *inode;
    if (!dir_lookup (dir, token, &inode))
    {
      dir_close (dir);
      return NULL;
    }

    dir_close (dir);
    dir = dir_open (inode);
    token = next_token;
  }

  if (strlen(token) == 0 || strlen(token) > NAME_MAX + 1) {
    return NULL;
  }

  strlcpy (file_name, token, NAME_MAX + 1);
  return dir;
}

// TODO: move to syscall.c?
bool filesys_chdir (const char *name)
{

  char file_name[NAME_MAX + 1];
  struct dir *dir = get_parent_directory(name, file_name);

  if (!dir) {
    return false;
  }

  struct thread* cur_thread = thread_current();
  dir_close (cur_thread->cwd);
  cur_thread->cwd = dir;
  return true;
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool filesys_create (const char *name, off_t initial_size, bool is_dir)
{
  block_sector_t inode_sector = 0;
  char file_name[NAME_MAX + 1];
  // struct dir dir* = dir_open_root ();
  struct dir *dir = get_parent_directory (name, file_name);

  // bool success = true;
  // if (dir == NULL) {
  //   success = false;
  // }
  // if (!free_map_allocate (1, &inode_sector)) {
  //   success = false;
  // }

  // if (!inode_create (inode_sector, initial_size, is_dir)) {
  //   success = false;
  // }

  // if (!dir_add (dir, file_name, inode_sector)) {
  //   success = false;
  // }

  // TODO: are we calling free_map_allocate twice?
  bool success = (dir != NULL && free_map_allocate (1, &inode_sector) &&
                  // TODO: do we need to modify any of the inode methods?
                  inode_create (inode_sector, initial_size, is_dir) &&
                  dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *filesys_open (const char *name)
{
  // TODO: check if length is 0?
  struct inode *inode = NULL;
  char file_name[NAME_MAX + 1];
  struct dir* dir = get_parent_directory(name, file_name);

  if (dir != NULL) {
    // if we're trying to open a file
    if (strlen(file_name) > 0) {
      dir_lookup (dir, file_name, &inode);
      dir_close (dir);
      // if we're trying to open a dir
    } else {
      inode = dir_get_inode (dir);
    }
  } else {
    return NULL;
  }

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool filesys_remove (const char *name)
{
  // edge case, should not be able to remove root
  if (strcmp("/", name) == 0) {
    return false;
  }
  char file_name[NAME_MAX + 1];
  struct dir* dir = get_parent_directory(name, file_name);

  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir);

  return success;
}

/* Formats the file system. */
static void do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}