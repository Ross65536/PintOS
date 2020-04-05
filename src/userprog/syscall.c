#include <stdio.h>
#include <syscall-nr.h>
#include <kernel/stdio.h>

#include "devices/shutdown.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "vm.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

#define SYSCALL_ERROR -1
#define MAX_FILENAME_SIZE 15

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static uint32_t get_stack_double_word (void** esp_adr) {
  void* esp = *esp_adr;
  bool success;
  uint32_t ret = get_userland_double_word (esp, &success);
  if (!success) {
    exit_curr_process (BAD_EXIT_CODE, true);
  }

  *esp_adr = increment_ptr (esp, 4); 
  return ret;
}

static inline int get_stack_int (void** esp_adr) {
  return (int) get_stack_double_word(esp_adr);
}

static inline void* get_stack_ptr (void** esp_adr) {
  return (void*) get_stack_double_word(esp_adr);
}

static inline void set_ret_val (struct intr_frame *f, int ret) {
  f->eax = ret;
}

static size_t write(int fd, char* buf, size_t size) {
  if (size > PGSIZE || fd == STDIN_FILENO) {
    return SYSCALL_ERROR;
  }

  if (fd == STDOUT_FILENO) {
    char* kbuf = malloc (size);
    if (kbuf == NULL) {
      return SYSCALL_ERROR;
    }

    if (! get_userland_buffer (buf, kbuf, size)) {
      free (kbuf); 
      return SYSCALL_ERROR;
    }

    putbuf (kbuf, size);
    free (kbuf);    
    return size;
  } 
  
  printf ("not implemented yet");
  exit_curr_process (BAD_EXIT_CODE, true);
  NOT_REACHED ();
}

static pid_t exec (char* cmd_line) {
  char c_cmd[MAX_PROCESS_ARGS_SIZE];
  size_t num_read;
  if (!get_userland_string(cmd_line, c_cmd, MAX_PROCESS_ARGS_SIZE, &num_read)) {
    exit_curr_process (BAD_EXIT_CODE, true);
  }

  if (num_read == MAX_PROCESS_ARGS_SIZE) {
    return PID_ERROR;
  }

  tid_t child = process_execute (c_cmd);
  if (child == TID_ERROR) {
    return PID_ERROR;
  }

  return child;
}

static bool create (char* file_path, unsigned int size) {
  char k_path[MAX_FILENAME_SIZE];
  size_t num_read;
  if (!get_userland_string(file_path, k_path, MAX_FILENAME_SIZE, &num_read)) {
    exit_curr_process (BAD_EXIT_CODE, true);
  }

  if (num_read == MAX_FILENAME_SIZE) { 
    return false;
  }

  lock_acquire (&filesys_monitor);
  const bool created = filesys_create (k_path, size);
  lock_release (&filesys_monitor);

  return created;
}

static bool remove (char* file_path) {
  char k_path[MAX_FILENAME_SIZE];
  size_t num_read;
  if (!get_userland_string(file_path, k_path, MAX_FILENAME_SIZE, &num_read)) {
    exit_curr_process (BAD_EXIT_CODE, true);
  }

  if (num_read == MAX_FILENAME_SIZE) { 
    return false;
  }

  lock_acquire (&filesys_monitor);
  const bool removed = filesys_remove (k_path);
  lock_release (&filesys_monitor);

  return removed;
}

static int open (char* file_path) {
  char k_path[MAX_FILENAME_SIZE];
  size_t num_read;
  if (!get_userland_string(file_path, k_path, MAX_FILENAME_SIZE, &num_read)) {
    exit_curr_process (BAD_EXIT_CODE, true);
  }

  if (num_read == MAX_FILENAME_SIZE) { 
    return SYSCALL_ERROR;
  }

  lock_acquire (&filesys_monitor);
  struct file * file = filesys_open (k_path);
  lock_release (&filesys_monitor);

  if (file == NULL) 
    return SYSCALL_ERROR;

  const int fd = add_process_open_file (thread_current ()->tid, file);

  return fd;
}

static void close(int fd) {
  if (fd < 0) {
    return;
  }

  process_close_file (thread_current ()->tid, fd);
}

static void
syscall_handler (struct intr_frame *f) 
{
  void* esp = f->esp;
  
  const int syscall_num = get_stack_int (&esp);
  switch (syscall_num) {
    // lab 2
    case SYS_HALT:
      shutdown_power_off ();
      break;
    case SYS_WRITE: {
      const int fd = get_stack_int (&esp);
      char* u_buf = (char*) get_stack_ptr (&esp);
      const size_t cont = get_stack_int (&esp);
      set_ret_val (f, write (fd, u_buf, cont));
      return;
    }
    case SYS_EXIT:
    {
      const int exit_code = get_stack_int (&esp);
      exit_curr_process (exit_code, true);
      break; 
    }
    case SYS_EXEC: {
      char* u_buf = (char*) get_stack_ptr (&esp);
      set_ret_val (f, exec (u_buf));
      return;
    }
    case SYS_WAIT: {
      const pid_t pid = get_stack_int (&esp);
      set_ret_val (f, process_wait (pid));
      return;
    }

    // lab 2 files
    case SYS_CREATE: {
      char* u_name = (char*) get_stack_ptr (&esp);
      unsigned int u_size = get_stack_double_word (&esp);
      set_ret_val (f, create (u_name, u_size));
      return; 
    }
    case SYS_REMOVE: {
      char* u_name = (char*) get_stack_ptr (&esp);
      set_ret_val (f, remove (u_name));
      return; 
    }
    case SYS_OPEN: {
      char* u_name = (char*) get_stack_ptr (&esp);
      set_ret_val (f, open (u_name));
      return; 
    }
    case SYS_CLOSE: {
      const int fd = get_stack_int (&esp);
      close (fd);
      return;
    }
    case SYS_FILESIZE:
    case SYS_READ:
    case SYS_SEEK:
    case SYS_TELL:

    // lab 3 & 4
    case SYS_MMAP:
    case SYS_MUNMAP:

    // lab 4
    case SYS_CHDIR:
    case SYS_MKDIR:
    case SYS_READDIR:
    case SYS_ISDIR:
    case SYS_INUMBER:
    default: {
      printf ("system call %d not implemented!\n", syscall_num); 
      exit_curr_process (BAD_EXIT_CODE, true);
    }
  }

  NOT_REACHED ();
}
 