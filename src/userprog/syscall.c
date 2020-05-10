#include <stdio.h>
#include <syscall-nr.h>
#include <kernel/stdio.h>
#include <kernel/console.h>

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
#include "process_vm.h"

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

static int write (int fd, char* buf, size_t size) {
  if (fd == STDIN_FILENO || fd < 0) {
    return SYSCALL_ERROR;
  }

  if (size == 0) {
    return 0;
  }

  char* kbuf = malloc (size);
  if (kbuf == NULL) {
    return SYSCALL_ERROR;
  }

  if (! get_userland_buffer (buf, kbuf, size)) {
    free (kbuf); 
    exit_curr_process (BAD_EXIT_CODE, true);
    NOT_REACHED ();
  }

  if (fd == STDOUT_FILENO) {
    putbuf (kbuf, size);
    free (kbuf);
    return size;
  } 

  struct file* file = get_process_open_file (find_current_thread_process (), fd);
  if (file == NULL) {
    free (kbuf);
    return SYSCALL_ERROR;
  }
  
  lock_acquire (&filesys_monitor);
  const int ret_size = file_write (file, kbuf, size);
  lock_release (&filesys_monitor);

  free (kbuf);    

  return ret_size;
}

static int read (int fd, char* buf, size_t size) {
  if (fd == STDOUT_FILENO || fd < 0) {
    return SYSCALL_ERROR;
  }

  if (size == 0) {
    return 0;
  }

  char* kbuf = malloc (size);
  if (kbuf == NULL) {
    return SYSCALL_ERROR;
  }

  int buf_size = -1;
  if (fd == STDIN_FILENO) {
    input_getchars ((uint8_t*) kbuf, size);
    buf_size = size;
  } else {

    struct file* file = get_process_open_file (find_current_thread_process (), fd);
    if (file == NULL) {
      free (kbuf);
      return SYSCALL_ERROR;
    }

    lock_acquire (&filesys_monitor);
    buf_size = file_read (file, kbuf, size); 
    lock_release (&filesys_monitor);
  }

  const bool ok_write = set_userland_buffer (buf, kbuf, buf_size);
  free (kbuf);    
  if (!ok_write) {
    exit_curr_process (BAD_EXIT_CODE, true);
    NOT_REACHED ();
  }

  return buf_size;
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

  return add_process_open_file (find_current_thread_process (), k_path);
}

static void close(int fd) {
  if (fd < 0) {
    return;
  }

  process_close_file (find_current_thread_process (), fd);
}

static int filesize (int fd) {
  if (fd < 0 || fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return SYSCALL_ERROR;
  }

  struct file* file = get_process_open_file (find_current_thread_process (), fd);
  if (file == NULL) {
    return SYSCALL_ERROR;
  }

  lock_acquire (&filesys_monitor);
  const int ret = file_length (file);
  lock_release (&filesys_monitor);

  return ret;
}

static int tell (int fd) {
  if (fd < 0 || fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return SYSCALL_ERROR;
  }

  struct file* file = get_process_open_file (find_current_thread_process (), fd);
  if (file == NULL) {
    return SYSCALL_ERROR;
  }

  lock_acquire (&filesys_monitor);
  const int ret = file_tell (file);
  lock_release (&filesys_monitor);

  return ret;
}

static void seek (int fd, unsigned int position) {
  if (fd < 0 || fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return exit_curr_process (BAD_EXIT_CODE, true);
  }

  struct file* file = get_process_open_file (find_current_thread_process (), fd);
  if (file == NULL) {
    return exit_curr_process (BAD_EXIT_CODE, true);
  }

  lock_acquire (&filesys_monitor);
  file_seek (file, position);
  lock_release (&filesys_monitor);
}

static int mmap(int fd, void* vaddr) {
  if (! is_page_aligned(vaddr) || vaddr == NULL) { 
    return SYSCALL_ERROR;
  }

  int ret = add_file_mapping (find_current_thread_process (), fd, vaddr);
  print_process_vm (find_current_thread_process ());
  print_process_mmaps(find_current_thread_process ());
  return ret;

}

static void munmap (int mmapid) { 
  unmap_file_mapping (find_current_thread_process (), mmapid);
}

static void
syscall_handler (struct intr_frame *f) 
{
  void* esp = f->esp;
  add_process_user_stack_ptr(find_current_thread_process(), esp);
  const int syscall_num = get_stack_int (&esp);
  switch (syscall_num) {
    // lab 2
    case SYS_HALT: {
      shutdown_power_off ();
      break;
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
    case SYS_READ: {
      const int fd = get_stack_int (&esp);
      char* u_buf = (char*) get_stack_ptr (&esp);
      const size_t cont = get_stack_double_word (&esp);
      set_ret_val (f, read (fd, u_buf, cont));
      return;
    }
    case SYS_WRITE: {
      const int fd = get_stack_int (&esp);
      char* u_buf = (char*) get_stack_ptr (&esp);
      const size_t cont = get_stack_int (&esp);
      set_ret_val (f, write (fd, u_buf, cont));
      return;
    }
    case SYS_FILESIZE: {
      const int fd = get_stack_int (&esp);
      set_ret_val (f, filesize (fd));
      return;
    }
    case SYS_SEEK: {
      const int fd = get_stack_int (&esp);
      const unsigned int position = get_stack_double_word (&esp);
      seek (fd, position);
      return;
    }
    case SYS_TELL: {
      const int fd = get_stack_int (&esp);
      set_ret_val (f, tell (fd));
      return;
    }

    // lab 3 & 4
    case SYS_MMAP: {
      const int fd = get_stack_int (&esp);
      void* v_addr =  get_stack_ptr (&esp);
      set_ret_val (f, mmap(fd, v_addr));
      return;
    }
    case SYS_MUNMAP: {
      const int mmapid = get_stack_int (&esp);
      munmap (mmapid);
      return;
    }

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
 