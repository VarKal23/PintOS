#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "vm/page_table.h"

static void syscall_handler (struct intr_frame *);
static void halt();
static void exit(void* esp);
static void exec(void* esp, struct intr_frame* f, char* cmd_line);
static void wait(void* esp, struct intr_frame* f);
static void create(void* esp, struct intr_frame* f, char* file_name);
static void remove(void* esp, struct intr_frame* f);
static void open(struct intr_frame* f, char* file_name);
static void write(void* esp, struct intr_frame* f, char* buf);
static void read(void* esp, struct intr_frame* f, char* buf);
static void filesize(void* esp, struct intr_frame* f);
static void seek(void* esp);
static void tell(void* esp, struct intr_frame* f);
static void close(void* esp);
struct lock file_lock;

// function to acquire the lock for manipulating files
// used in process.c
// Matthew Driving
void acquire_filesys_lock () {
  lock_acquire (&file_lock);
}

// function to release the lock for manipulating files
void release_filesys_lock () {
  lock_release (&file_lock);
}

void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

// function to validate a pointer. ensures that the pointer is not 
// null doesn't point to unmapped virtual memory, and doesn't 
// point to kernel virtual address space
bool valid_pointer (void *pointer, void *esp) {
  if (!pointer || !is_user_vaddr (pointer) 
               || !pagedir_get_page (thread_current()->pagedir, pointer)) {
    return false;
  }
  bool valid = false;
  struct page_entry* page = get_page(pointer);
  if (page) {
    if (!page->frame) {
      // valid = lock_page(page);
      valid = load_page(page);
    } else {
      return true;
    }
  } else if (is_accessing_stack(esp, pointer)) {
    valid = grow_stack(pointer);
  }
  return valid;
}

bool valid_string(char *str, void *esp) {
  // if (!valid_pointer(str, esp)) return false;
  while (*str != 0)
    {
      if (!str || !is_user_vaddr (str)) {
        return false;
      }
      bool valid = false;
      struct page_entry* page = get_page(str);
      if (page != NULL) {
            if (!page->frame) {
              // valid = lock_page(page);
              valid = load_page(page);
            } else {
              valid = true;
            }
      } else if (is_accessing_stack(esp, str)) {
            valid = grow_stack(str);
      }
      if (!valid) {
        return false;
      }
      str++;
    }
  return true;
}

bool valid_buffer (void* buffer, unsigned size, void* esp) {
  char* buffer_cpy = buffer;
  for (int i = 0; i < size; i++)
    {
      if (!buffer_cpy || !is_user_vaddr (buffer_cpy)) {
        return false;
      }
      bool valid = false;
      struct page_entry* page = get_page(buffer_cpy);
      if (page != NULL) {
            if (!page->frame) {
              // valid = lock_page(page);
              valid = load_page(page);
            } else {
              valid = true;
            }
      } else if (is_accessing_stack(esp, buffer_cpy)) {
            valid = grow_stack(buffer_cpy);
      }
      if (!valid) {
        return false;
      }
      buffer_cpy++;
    }
}

void release_pointer(void *pointer) {
  struct page_entry* page = get_page(pointer);
  unlock_page(page);
}

void release_string(char *str) {
  while (*str != 0)
    {
      struct page_entry* page = get_page(str);
      unlock_page(page);
      str++;
    }
}

void release_buffer(void* buffer, unsigned size) {
  char* buffer_cpy = buffer;
  // you don't need to iterate over every char
  for (int i = 0; i < size; i+= PGSIZE)
    {
      struct page_entry* page = get_page(buffer_cpy);
      unlock_page(page);
      buffer_cpy++;
    }
}

// validates all the arguments of a system call
bool valid_args (char* base, int num_args) {
  for (int i = 0; i < num_args; i++) {
    // each arg takes up 4 bytes
    if (!valid_pointer (base + 4*i, base)) return false;
  }
  return true;
}
// Varun Driving
static void syscall_handler (struct intr_frame *f UNUSED)
{
  void *esp = f->esp;
  if (!valid_pointer (esp, esp)) {
    thread_exit (-1);
    return;
  }
  // syscall number is stored at esp, args begin at esp + 4
  int syscall_number = *(int *) esp;
  esp = (char*) esp + 4;

  // Validates arguments of various arg count
  if (syscall_number == SYS_CREATE || SYS_SEEK) {
    if (!valid_args(esp, 2)) {
      thread_exit(-1);
      return;
    }
  } else if (syscall_number == SYS_WRITE || SYS_READ) {
    if (!valid_args(esp, 3)) {
      thread_exit(-1);
      return;
    }
  // Matthew Driving
  } else {
    if (!valid_args(esp, 1)) {
      thread_exit(-1);
      return;
    }
  }

  if (syscall_number == SYS_HALT) {
    halt();

  } else if (syscall_number == SYS_EXIT) {
    exit(esp);
  // Varun Driving
  } else if (syscall_number == SYS_EXEC) {
    char* cmd_line = *(char **) esp;
    if (!valid_string(cmd_line, esp)) {
      thread_exit(-1);
      return;
    }
    exec(esp, f, cmd_line);
    release_string(cmd_line);

  } else if (syscall_number == SYS_WAIT) {
    wait(esp, f);

  } else if (syscall_number == SYS_CREATE) {
    char* file_name = *(char**) esp;
    if (!valid_string (file_name, esp)) {
      thread_exit (-1);
      return;
    }
    create(esp, f, file_name);
    release_string(file_name);

  } else if (syscall_number == SYS_REMOVE) {
    remove(esp, f);
  // Matthew Driving
  } else if (syscall_number == SYS_OPEN) {
    char* file_name = *(char**) esp;
    if (!valid_string (file_name, esp)) {
      thread_exit (-1);
      return;
    }
    open(f, file_name);
    release_string(file_name);

  } else if (syscall_number == SYS_WRITE) {
    char* buf = *(char**) ((char*) esp + 4);
    write(esp, f, buf);
  // Varun Driving
  } else if (syscall_number == SYS_READ) {
    char* buf = *(char**) ((char*) esp + 4);
    read(esp, f, buf);

  } else if (syscall_number == SYS_FILESIZE) {
    filesize(esp, f);

  } else if (syscall_number == SYS_SEEK) {
    seek(esp);

  } else if (syscall_number == SYS_TELL) {
    tell(esp, f);

  } else if (syscall_number == SYS_CLOSE) {
    close(esp);
  }
}

// function to perform halt
static void halt() {
  shutdown_power_off ();
}

// function to perform exit
static void exit(void* esp) {
  int status = *(int *) esp;
  thread_exit (status);
}

// Varun Driving
// function to perform exec
static void exec(void* esp, struct intr_frame* f, char* cmd_line) {
  f->eax = process_execute (cmd_line);
}

// function to perform wait
static void wait(void* esp, struct intr_frame* f) {
  tid_t tid = *(tid_t *) esp;
  f->eax = process_wait (tid);
}

// Matthew Driving
// function to perform create
static void create(void* esp, struct intr_frame* f, char* file_name) {
  unsigned int initial_size = *(unsigned int*) ((char*) esp + 4);
  lock_acquire (&file_lock);
  f->eax = filesys_create (file_name, initial_size);
  lock_release (&file_lock);
}

// function to perform remove
static void remove(void* esp, struct intr_frame* f) {
  char* file_name = *(char**) esp;
  lock_acquire (&file_lock);
  f->eax = filesys_remove (file_name);
  lock_release (&file_lock);
}

// Varun Driving
// function to perform open
static void open(struct intr_frame* f, char* file_name) {
  lock_acquire (&file_lock);
  struct file* fp = filesys_open (file_name);
  lock_release (&file_lock);

  struct thread* cur_thread = thread_current();
  if (!fp) {
    f->eax = -1;
  } else {
    int index = 2;
    while (cur_thread->fdt[index] != NULL) {
      index++;
    }
    cur_thread->fdt[index] = fp;
    f->eax = index;
  } 
}

// Matthew Driving
// function to perform write
static void write(void* esp, struct intr_frame* f, char* buf) {
  int fd = *(int *) esp;
  if (fd > 128 || fd < 0) {
    thread_exit (-1);
    return;
  }
  unsigned size = *(unsigned*) ((char*) esp + 8);
  if (!valid_buffer (buf, size, f->esp)) {
    thread_exit (-1);
    return;
  }
  if (fd == 1) {
    lock_acquire (&file_lock);
    putbuf (buf, size);
    lock_release (&file_lock);
    f->eax = size;
  } else {
    struct file* file = thread_current ()->fdt[fd];
    if (!file) {
      f->eax = -1;
    } else {
      lock_acquire (&file_lock);
      f->eax = file_write (file, buf, size);
      lock_release (&file_lock);
    }
  }
  release_buffer(buf, size);
}

// function to perform read
static void read(void* esp, struct intr_frame* f, char* buf) {
  int fd = *(int *) esp;
  if (fd > 128 || fd < 0) {
    thread_exit (-1);
    return;
  }
  unsigned size = *(unsigned*) ((char*) esp + 8);
  if (!valid_buffer (buf, size, f->esp)) {
    thread_exit (-1);
    return;
  }
  if (fd == 0) {
    lock_acquire (&file_lock);
    for (int i = 0; i < size; i++) {
      buf[i] = input_getc ();
    }
    lock_release (&file_lock);
    f->eax = size;
  } else {
    // Varun Driving
    struct file* file = thread_current ()->fdt[fd];
    if (!file) {
      f->eax = -1;
    } else {
      lock_acquire (&file_lock);
      f->eax = file_read (file, buf, size);
      lock_release (&file_lock);
    }
  }
  release_buffer(buf, size);
}

// Matthew Driving
// function to perform filesize
static void filesize(void* esp, struct intr_frame* f) {
  int fd = *(int *) esp;
  if (fd > 128 || fd < 0) {
    thread_exit (-1);
    return;
  }
  struct file* file = thread_current ()->fdt[fd];
  if (!file) {
    f->eax = -1;
  } else {
    lock_acquire (&file_lock);
    f->eax = file_length (file);
    lock_release (&file_lock);
  }
}

// Varun Driving
// function to perform seek
static void seek(void* esp) {
  int fd = *(int *) esp;
  if (fd > 128 || fd < 0) {
    thread_exit (-1);
    return;
  }
  unsigned int pos = *(unsigned int*) ((char*) esp + 4);
  struct file* file = thread_current ()->fdt[fd];
  if (file) {
    lock_acquire (&file_lock);
    file_seek(file, pos);
    lock_release (&file_lock);
  }
}

// function to perform tell
static void tell(void* esp, struct intr_frame* f) {
  int fd = *(int *) esp;
  if (fd > 128 || fd < 0) {
    thread_exit (-1);
    return;
  }
  struct file* file = thread_current ()->fdt[fd];
  if (!file) {
    f->eax = -1;
  } else {
    lock_acquire (&file_lock);
    f->eax = file_tell(file);
    lock_release (&file_lock);
  }
}

// Matthew Driving
// function to perform close
static void close(void* esp) {
  int fd = *(int *) esp;
  if (fd > 128 || fd < 0) {
    thread_exit (-1);
    return;
  }
  struct file* file = thread_current ()->fdt[fd];
  if (file) {
    lock_acquire (&file_lock);
    file_close(file);
    thread_current ()->fdt[fd] = NULL;
    lock_release (&file_lock);
  }
}
