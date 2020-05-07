#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "userprog/process.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm.h"
#include "threads/synch.h"
#include "vm/active_files.h"
#include "vm/strings_pool.h"
#include "vm/file_page.h"
#include "process_vm.h"
#include "process_impl.h"

#define MAX_ARGS 30

static thread_func start_process NO_RETURN;


struct start_process_arg {
  char command[MAX_PROCESS_ARGS_SIZE];
  size_t num_args;
  char* args[MAX_ARGS];
  tid_t parent_tid;
  
  // synch
  struct semaphore created_sema;
  bool child_failed;
};

static struct file* load (struct start_process_arg *start_process_arg, void (**eip) (void), void **esp);

static void parse_executable_command (struct start_process_arg* process_args, const char* command) {
  process_args->parent_tid = current_thread_tid ();
  process_args->child_failed = false;
  strlcpy (process_args->command, command, MAX_PROCESS_ARGS_SIZE);
  sema_init (&process_args->created_sema, 0);

  process_args->num_args = 0;
  
  for (char *save_ptr, *token = strtok_r (process_args->command, " ", &save_ptr); 
          process_args->num_args < MAX_ARGS && token != NULL; 
          token = strtok_r (NULL, " ", &save_ptr)) {

        process_args->args[process_args->num_args] = token;
        process_args->num_args++;
  }

  ASSERT (process_args->num_args > 0);
}


/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *exec_command) 
{
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  struct start_process_arg *start_process_arg = palloc_get_page (0);
  if (start_process_arg == NULL)
    return TID_ERROR;
  
  parse_executable_command (start_process_arg, exec_command);
  char name[PROCESS_MAX_NAME];
  strlcpy (name, start_process_arg->args[0], PROCESS_MAX_NAME);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (exec_command, PRI_DEFAULT, start_process, start_process_arg);
  if (tid == TID_ERROR) {
    palloc_free_page (start_process_arg); 
    return TID_ERROR;
  }

  sema_down (&start_process_arg->created_sema);
  const bool failed = start_process_arg->child_failed;
  palloc_free_page (start_process_arg);
  if (failed) {
    return TID_ERROR;
  }

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *arg)
{
  struct start_process_arg *start_process_arg = arg;
  struct intr_frame if_;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  struct file* exec_file = load (start_process_arg, &if_.eip, &if_.esp);

  // print_active_files();
  // print_strings_pool();
  // print_process_vm(find_current_thread_process());
  // print_frame_table();
  
  /* If load failed, quit. */
  if (exec_file == NULL) {
    start_process_arg->child_failed = true;
    sema_up (&start_process_arg->created_sema);
    
    thread_exit ();
  }

  start_process_arg->child_failed = false;
  sema_up (&start_process_arg->created_sema);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  if (child_tid == TID_ERROR) {
    return BAD_EXIT_CODE;
  }

  return collect_process_exit_code(find_process (child_tid));
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool validate_segment (const struct Elf32_Phdr *, struct file *);

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file UNUSED, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable, const char* file_name, struct process_node* process_node) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      struct vm_node* node = add_file_backed_vm(process_node, upage, file_name, ofs, page_zero_bytes, !writable, true);
      if (node == NULL) {
        return false;
      }

      ofs += page_read_bytes;
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

static int load_stack_args (uint8_t * kpage_bottom, uint8_t* upage_bottom, char* args[], size_t num_args) {

  uint8_t * kpage_top = kpage_bottom + PGSIZE;
  uint8_t * upage_top = upage_bottom + PGSIZE;

  const size_t uargs_size = num_args + 1;
  ASSERT (uargs_size <= MAX_ARGS + 1);
  uint8_t* u_args[uargs_size];
  u_args[uargs_size - 1] = NULL; // c's argv[] ends in NULL

  size_t offset = 0;

  for (size_t i = 0; i < num_args; i++) {
    char* arg = args[i];

    const size_t arg_offset = strlen (arg) + 1; // also count \0
    offset += arg_offset;

    strlcpy ((char*) (kpage_top - offset), arg, arg_offset);
    u_args[i] = upage_top - offset;
  }

  // align pointers for arguments
  offset += pointer_alignment_offset(kpage_top - offset, CALL_ARG_ALIGNMENT);

  const size_t argv_arr_offset = uargs_size * sizeof (uint8_t *);
  offset += argv_arr_offset;
  memcpy (kpage_top - offset, u_args, argv_arr_offset);

  const size_t argv_offset = CALL_ARG_ALIGNMENT;
  const uint32_t u_argv_ptr =(uint32_t) upage_top - offset;   
  offset += argv_offset;
  int* argv_ptr = (int*) (kpage_top - offset);
  *argv_ptr = u_argv_ptr;

  const size_t argc_offset = CALL_ARG_ALIGNMENT;
  offset += argc_offset;
  int* argc_ptr = (int*) (kpage_top - offset);
  *argc_ptr = num_args;

  const size_t null_ret_offset = CALL_ARG_ALIGNMENT;
  offset += null_ret_offset;

  return offset;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (struct process_node* process, void **esp, char* args[], size_t num_args) 
{
  uint8_t * upage_bottom = ((uint8_t *) PHYS_BASE) - PGSIZE;

  struct vm_node* vm_node = add_freestanding_vm(process, upage_bottom);
  if (vm_node == NULL) {
    return false;
  }

  uint8_t *kpage = (uint8_t *) activate_vm_page(vm_node);
  if (kpage == NULL) {
    return false;
  }

  const size_t offset = load_stack_args (kpage, upage_bottom, args, num_args);
  *esp = PHYS_BASE - offset;
  
  return true;
}


/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns file used as executable if successful, NULL otherwise. */
static struct file* load (struct start_process_arg *start_process_arg, void (**eip) (void), void **esp) 
{
  char** args = start_process_arg->args;
  size_t num_args = start_process_arg->num_args;
  const char *file_name = args[0];

  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  struct process_node* process_node = NULL;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  lock_acquire (&filesys_monitor);

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  file_deny_write (file);
  pid_t process_id = current_thread_tid ();
  process_node = add_process (start_process_arg->parent_tid, process_id, thread_current()->pagedir, start_process_arg->args[0], file);
  if (process_node == NULL) {
    goto done;
  }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              // printf("phdr.p_offset=%d phdr.p_vaddr=%x phdr.p_filesz=%d\n", phdr.p_offset, phdr.p_vaddr, phdr.p_filesz);
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable, file_name, process_node))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (process_node, esp, args, num_args)) {
    goto done;
  }

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  if (! success) {
    file_close (file);
    lock_release (&filesys_monitor);
    if (process_node != NULL) {
      process_add_exit_code(process_node, BAD_EXIT_CODE);
      ASSERT (collect_process_exit_code(process_node) == BAD_EXIT_CODE);
    }
    return NULL;
  }

  lock_release (&filesys_monitor);
  return file;
}

/* load() helpers. */


/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}




