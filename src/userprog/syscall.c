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
  lock_init(&file_lock);
}

bool valid_pointer (void* pointer) {
  return pointer && is_user_vaddr (pointer) && pagedir_get_page (thread_current()->pagedir, pointer);
}

bool valid_args (char* base, int num_args) {
  for (int i = 0; i < num_args; i++) {
    // each arg takes up 4 bytes
    if (!valid_pointer(base + 4*i)) return false;
  }
  return true;
}

static void syscall_handler (struct intr_frame *f UNUSED)
{
  void *esp = f->esp;
  if (!valid_pointer (esp)) {
    // we can also call thread_exit
    // but what status do we pass?
    process_exit (-1);
    return;
  }
  // syscall number is stored at esp, args begin at esp + 4
  int syscall_number = *(int *) esp;
  esp = (char*) esp + 4;

  if (syscall_number == SYS_HALT) {
    shutdown_power_off();

  } else if (syscall_number == SYS_EXIT) {
    if (!valid_args (esp, 1)) {
      process_exit (-1);
      return;
    }
    int status = *(int *) esp;
    printf("%d", status);
    process_exit (status);

  } else if (syscall_number == SYS_EXEC) {
    if (!valid_args (esp, 1)) {
      process_exit (-1);
      return;
    }
    char* cmd_line = *(char **) esp;
    f->eax = process_execute(cmd_line);

  } else if (syscall_number == SYS_WAIT) {
    if (!valid_args (esp, 1)) {
      process_exit (-1);
      return;
    }
    tid_t tid = *(tid_t *) esp;
    f->eax = process_wait(tid);

  } else if (syscall_number == SYS_CREATE) {
     if (!valid_args (esp, 2)) {
      process_exit (-1);
      return;
    }
    
    char* file_name = *(char**) esp;
    unsigned int initial_size = *(unsigned int*) esp;
    lock_acquire(&file_lock);
    f->eax = filesys_create(file_name, initial_size);
    lock_release(&file_lock);

  } else if (syscall_number == SYS_REMOVE) {
     if (!valid_args (esp, 1)) {
      process_exit (-1);
      return;
    }

    char* file_name = *(char**) esp;
    lock_acquire(&file_lock);
    f->eax = filesys_remove(file_name);
    lock_release(&file_lock);

  } else if (syscall_number == SYS_OPEN) {
    if (!valid_args (esp, 1)) {
      process_exit (-1);
      return;
    }
    char* file_name = *(char**) esp;
    lock_acquire(&file_lock);
    struct file* fp = filesys_open(file_name);
    lock_release(&file_lock);

    struct thread* cur_thread = thread_current();
    if (!fp) {
      f->eax = -1;
    } else {
      int index = 2;
      // TODO: do we need to handle case of running out of fdt spots?
      while(cur_thread->fdt[index] != NULL) {
        index++;
      }
      cur_thread->fdt[index] = fp;
      f->eax = index;
    } 

  } else if (syscall_number == SYS_WRITE) {
    if (!valid_args (esp, 3)) {
      process_exit (-1);
      return;
    }

    int fd = *(int *) esp;
    void* buf = (void*) esp;
    unsigned size = *(unsigned*) esp;

    printf("%d", fd);

    if (fd == 1) {
      putbuf(buf, size);
      f->eax = size;
    } else {
      struct file* open_file = thread_current()->fdt[fd];
      if (!open_file) {
        f->eax = -1;
        return;
      } 
      lock_acquire(&file_lock);
      f->eax = file_write(open_file, buf, size);
      lock_release(&file_lock);
    }

  }

}
