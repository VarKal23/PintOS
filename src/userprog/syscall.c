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
static struct lock file_lock;

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
    // TODO: call process exit?
    // and with what status?
    // remember you changed the call in some other files too
    thread_exit (-1);
    return;
  }
  // syscall number is stored at esp, args begin at esp + 4
  int syscall_number = *(int *) esp;
  esp = (char*) esp + 4;

  if (syscall_number == SYS_HALT) {
    shutdown_power_off ();

  } else if (syscall_number == SYS_EXIT) {
    if (!valid_args (esp, 1)) {
      thread_exit (-1);
      return;
    }
    int status = *(int *) esp;
    // printf("%d", status);
    thread_exit (status);

  } else if (syscall_number == SYS_EXEC) {
    if (!valid_args (esp, 1)) {
      thread_exit (-1);
      return;
    }
    char* cmd_line = *(char **) esp;
    f->eax = process_execute (cmd_line);

  } else if (syscall_number == SYS_WAIT) {
    if (!valid_args (esp, 1)) {
      thread_exit (-1);
      return;
    }
    tid_t tid = *(tid_t *) esp;
    f->eax = process_wait (tid);

  } else if (syscall_number == SYS_CREATE) {
     if (!valid_args (esp, 2)) {
      thread_exit (-1);
      return;
    }
    
    char* file_name = *(char**) esp;
    if (!valid_pointer (file_name)) {
      thread_exit (-1);
      return;
    }
    unsigned int initial_size = *(unsigned int*) ((char*) esp + 4);
    lock_acquire (&file_lock);
    f->eax = filesys_create (file_name, initial_size);
    lock_release (&file_lock);

  } else if (syscall_number == SYS_REMOVE) {
     if (!valid_args (esp, 1)) {
      thread_exit (-1);
      return;
    }

    char* file_name = *(char**) esp;
    lock_acquire (&file_lock);
    f->eax = filesys_remove (file_name);
    lock_release (&file_lock);

  } else if (syscall_number == SYS_OPEN) {
    if (!valid_args (esp, 1)) {
      thread_exit (-1);
      return;
    }
    char* file_name = *(char**) esp;
    if (!valid_pointer (file_name)) {
      thread_exit (-1);
      return;
    }
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

  } else if (syscall_number == SYS_WRITE) {
    if (!valid_args (esp, 3)) {
      thread_exit (-1);
      return;
    }

    int fd = *(int *) esp;
    // TODO: increment esp pointer?
    char* buf = *(char**) ((char*) esp + 4);
    if (!valid_pointer (buf)) {
      thread_exit (-1);
      return;
    }
    unsigned size = *(unsigned*) ((char*) esp + 8);
    // TODO: validate buffer?

    if (fd == 1) {
      putbuf (buf, size);
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
  } else if (syscall_number == SYS_READ) {
    // TODO: change up this function
    if (!valid_args (esp, 3)) {
      thread_exit (-1);
      return;
    }
    int fd = *(int *) esp;
    char* buf = *(char**) ((char*) esp + 4);
    if (!valid_pointer (buf)) {
      thread_exit (-1);
      return;
    }
    unsigned size = *(unsigned*) ((char*) esp + 8);
    if (fd == 0) {
      for (int i = 0; i < size; i++) {
        buf[i] = input_getc ();
      }
      f->eax = size;
    } else {
      struct file* file = thread_current ()->fdt[fd];
      // printf("%p", file);
      if (!file) {
        f->eax = -1;
      } 
      lock_acquire (&file_lock);
      f->eax = file_read (file, buf, size);
      lock_release (&file_lock);
    }
  } else if (syscall_number == SYS_FILESIZE) {
    if (!valid_args (esp, 1)) {
      thread_exit (-1);
      return;
    }

    int fd = *(int *) esp;
    struct file* file = thread_current ()->fdt[fd];
    if (!file) {
      f->eax = -1;
    }
    // TODO: move locks?
    lock_acquire (&file_lock);
    f->eax = file_length (file);
    lock_release (&file_lock);
  } else if (syscall_number == SYS_SEEK) {
    if (!valid_args (esp, 2)) {
      thread_exit (-1);
      return;
    }
    int fd = *(int *) esp;
    unsigned int pos = *(unsigned int*) ((char*) esp + 4);
    struct file* file = thread_current ()->fdt[fd];
    if (file) {
      lock_acquire (&file_lock);
      file_seek(file, pos);
      lock_release (&file_lock);
    }

  } else if (syscall_number == SYS_TELL) {
    if (!valid_args (esp, 1)) {
      thread_exit (-1);
      return;
    }

    int fd = *(int *) esp;
    struct file* file = thread_current ()->fdt[fd];
    if (!file) {
      f->eax = -1;
    } else {
      lock_acquire (&file_lock);
      f->eax = file_tell(file);
      lock_release (&file_lock);
    }

  } else if (syscall_number == SYS_CLOSE) {
    if (!valid_args (esp, 1)) {
      thread_exit (-1);
      return;
    }
    int fd = *(int *) esp;
    struct file* file = thread_current ()->fdt[fd];
    if (file) {
      lock_acquire (&file_lock);
      file_close(file);
      thread_current ()->fdt[fd] = NULL;
      lock_release (&file_lock);
      // free(file);
    }
  }
}
