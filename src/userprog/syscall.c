#include <stdio.h>
#include <syscall-nr.h>

#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process_exit.h"
#include "vm.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static int get_stack_int (void** esp_adr) {
  void* esp = *esp_adr;
  bool success;
  int ret = get_userpage_int (esp, &success);
  if (!success) {
    abnormal_process_exit ();
  }

  *esp_adr = increment_ptr (esp, 4); 
  return ret;
}

static void
syscall_handler (struct intr_frame *f) 
{
  void* esp = f->esp;
  int syscall_num = get_stack_int (&esp);

  printf ("system call %d!\n", syscall_num); 
  tid_t tid = thread_current ()->tid;
  process_add_exit_code (tid, 0); // TODO fixup
  print_exit_code (tid);


  thread_exit ();
}
 