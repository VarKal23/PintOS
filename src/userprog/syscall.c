#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

bool valid_pointer (void* pointer) {
  return pointer && is_user_vaddr (pointer) && pagedir_get_page (thread_current()->pagedir, pointer);
}

bool valid_args (void* base, int num_args) {
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
  esp += 4;
  if (syscall_number == SYS_HALT) {
    shutdown_power_off();
  } else if (syscall_number == SYS_EXIT) {
    if (!valid_args (esp, 1)) {
      process_exit (-1);
      return;
    }
    int status = *(int *) esp;
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
  }
}
