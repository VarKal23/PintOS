#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);
static void halt();
static void exit(void* esp);
static void exec(void* esp, struct intr_frame* f);
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

void acquire_file_lock () {
  lock_acquire (&file_lock);
}

void release_file_lock () {
  lock_release (&file_lock);
}

void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

bool valid_pointer (void* pointer) {
  return pointer && is_user_vaddr (pointer) && pagedir_get_page (thread_current()->pagedir, pointer);
}

bool valid_args (char* base, int num_args) {
  for (int i = 0; i < num_args; i++) {
    // each arg takes up 4 bytes
    if (!valid_pointer (base + 4*i)) return false;
  }
  return true;
}

static void syscall_handler (struct intr_frame *f UNUSED)
{
  void *esp = f->esp;
  if (!valid_pointer (esp)) {
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

  } else if (syscall_number == SYS_EXEC) {
    exec(esp, f);

  } else if (syscall_number == SYS_WAIT) {
    wait(esp, f);

  } else if (syscall_number == SYS_CREATE) {
    char* file_name = *(char**) esp;
    if (!valid_pointer (file_name)) {
      thread_exit (-1);
      return;
    }
    create(esp, f, file_name);

  } else if (syscall_number == SYS_REMOVE) {
    remove(esp, f);

  } else if (syscall_number == SYS_OPEN) {
    char* file_name = *(char**) esp;
    if (!valid_pointer (file_name)) {
      thread_exit (-1);
      return;
    }
    open(f, file_name);

  } else if (syscall_number == SYS_WRITE) {
    char* buf = *(char**) ((char*) esp + 4);
    if (!valid_pointer (buf)) {
      thread_exit (-1);
      return;
    }
    write(esp, f, buf);

  } else if (syscall_number == SYS_READ) {
    char* buf = *(char**) ((char*) esp + 4);
    if (!valid_pointer (buf)) {
      thread_exit (-1);
      return;
    }
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

static void halt() {
  shutdown_power_off ();
}

static void exit(void* esp) {
  int status = *(int *) esp;
  thread_exit (status);
}

static void exec(void* esp, struct intr_frame* f) {
  char* cmd_line = *(char **) esp;
  f->eax = process_execute (cmd_line);
}

static void wait(void* esp, struct intr_frame* f) {
  tid_t tid = *(tid_t *) esp;
  f->eax = process_wait (tid);
}

static void create(void* esp, struct intr_frame* f, char* file_name) {
  unsigned int initial_size = *(unsigned int*) ((char*) esp + 4);
  lock_acquire (&file_lock);
  f->eax = filesys_create (file_name, initial_size);
  lock_release (&file_lock);
}

static void remove(void* esp, struct intr_frame* f) {
  char* file_name = *(char**) esp;
  lock_acquire (&file_lock);
  f->eax = filesys_remove (file_name);
  lock_release (&file_lock);
}

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

static void write(void* esp, struct intr_frame* f, char* buf) {
  int fd = *(int *) esp;
  unsigned size = *(unsigned*) ((char*) esp + 8);
  if (fd == 1) {
    lock_acquire (&file_lock);
    putbuf (buf, size);
    lock_release (&file_lock);
    f->eax = size;
  } else {
    struct file* file = thread_current ()->fdt[fd];
    if (!file) {
      f->eax = -1;
    } 
    lock_acquire (&file_lock);
    f->eax = file_write (file, buf, size);
    lock_release (&file_lock);
  }
}

static void read(void* esp, struct intr_frame* f, char* buf) {
  int fd = *(int *) esp;
  unsigned size = *(unsigned*) ((char*) esp + 8);
  if (fd == 0) {
    lock_acquire (&file_lock);
    for (int i = 0; i < size; i++) {
      buf[i] = input_getc ();
    }
    lock_release (&file_lock);
    f->eax = size;
  } else {
    struct file* file = thread_current ()->fdt[fd];
    if (!file) {
      f->eax = -1;
    } 
    lock_acquire (&file_lock);
    f->eax = file_read (file, buf, size);
    lock_release (&file_lock);
  }
}

static void filesize(void* esp, struct intr_frame* f) {
  int fd = *(int *) esp;
  struct file* file = thread_current ()->fdt[fd];
  if (!file) {
    f->eax = -1;
  }
  lock_acquire (&file_lock);
  f->eax = file_length (file);
  lock_release (&file_lock);
}

static void seek(void* esp) {
  int fd = *(int *) esp;
  unsigned int pos = *(unsigned int*) ((char*) esp + 4);
  struct file* file = thread_current ()->fdt[fd];
  if (file) {
    lock_acquire (&file_lock);
    file_seek(file, pos);
    lock_release (&file_lock);
  }
}

static void tell(void* esp, struct intr_frame* f) {
  int fd = *(int *) esp;
  struct file* file = thread_current ()->fdt[fd];
  if (!file) {
    f->eax = -1;
  } else {
    lock_acquire (&file_lock);
    f->eax = file_tell(file);
    lock_release (&file_lock);
  }
}

static void close(void* esp) {
  int fd = *(int *) esp;
  struct file* file = thread_current ()->fdt[fd];
  if (file) {
    lock_acquire (&file_lock);
    file_close(file);
    thread_current ()->fdt[fd] = NULL;
    lock_release (&file_lock);
  }
}
