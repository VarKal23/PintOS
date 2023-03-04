#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void acquire_filesys_lock ();
void release_filesys_lock ();

#endif /* userprog/syscall.h */