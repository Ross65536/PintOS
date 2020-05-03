#include <debug.h>
#include <string.h>
#include <stdio.h>
#include <kernel/list.h>
#include <kernel/hash.h>

#include "process.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "filesys/file.h"
#include "process_vm.h"
#include "vm/frame_table.h"
#include "vm/file_page.h"
#include "vm/active_files.h"
#include "vm/page_common.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "process_impl.h"

struct lock filesys_monitor;

struct process_node {
  struct lock lock;

  struct hash_elem elem;
  char name[PROCESS_MAX_NAME];
  tid_t parent_pid;
  tid_t pid; // the process pid, same as thread tid
  uint32_t* pagedir;

  // exit codes
  struct condition cond_exited;
  bool exited;
  int exit_code;

  // files
  int fd_counter;
  struct list open_files;
  struct file* exec_file;

  // supplemental VM table
  struct hash vm_table;
};

static struct processes {
  struct hash processes;
  struct lock lock;
} processes;

static unsigned int processes_hash_func (const struct hash_elem *e, void *_ UNUSED) {
  struct process_node *p = hash_entry (e, struct process_node, elem);
  return hash_int (p->pid);
}

static bool processes_hash_less_func (const struct hash_elem *l, const struct hash_elem *r, void *_ UNUSED) {
  struct process_node *pl = hash_entry (l, struct process_node, elem);
  struct process_node *pr = hash_entry (r, struct process_node, elem);

  return pl->pid < pr->pid;
}

void 
process_impl_init () {
  const bool ok = hash_init (&processes.processes, processes_hash_func, processes_hash_less_func, NULL);
  ASSERT (ok);
  lock_init (&processes.lock);
  lock_init (&filesys_monitor);
}

static struct process_node* find_process_internal (pid_t pid) {

  struct process_node node_find;
  node_find.pid = pid;
  struct hash_elem * found = hash_find (&processes.processes, &node_find.elem); 
  if (found == NULL)
    return NULL;

  return hash_entry (found, struct process_node, elem);
}

struct process_node* find_process (pid_t pid) {
  lock_acquire (&processes.lock);
  struct process_node* node = find_process_internal (pid);
  lock_release (&processes.lock);
  return node;
}

struct process_node* find_current_thread_process () {
  pid_t process_id = current_thread_tid ();
  return find_process(process_id);
}



static void remove_process (struct process_node* node) {
  ASSERT (hash_delete (&processes.processes, &node->elem) != NULL);
  free (node);
}

static unsigned int vm_table_hash (const struct hash_elem *e, void *_ UNUSED);
static bool vm_table_less (const struct hash_elem *l, const struct hash_elem *r, void *_ UNUSED);
 
struct process_node* add_process (pid_t parent_tid, pid_t tid, uint32_t* pagedir, const char* name, struct file* exec_file) {
  ASSERT (! intr_context());

  lock_acquire (& processes.lock);

  if (find_process_internal (tid) != NULL) {
    PANIC("Process already exists");
  }

  struct process_node* node = malloc (sizeof (struct process_node));
  if (node == NULL) {
    lock_release (& processes.lock);
    return NULL;
  }

  strlcpy (node->name, name, PROCESS_MAX_NAME);
  node->parent_pid = parent_tid;
  node->pid = tid;
  node->exited = false;
  node->exit_code = BAD_EXIT_CODE;
  node->pagedir = pagedir;
  cond_init(&node->cond_exited);
  list_init (&node->open_files);
  node->fd_counter = 2;
  node->exec_file = exec_file;
  lock_init(&node->lock);
  if (! hash_init(&node->vm_table, vm_table_hash, vm_table_less, NULL)) {
    free (node);
    lock_release (& processes.lock);
    return NULL;
  }

  ASSERT (hash_insert (& processes.processes, &node->elem) == NULL);

  lock_release (& processes.lock);

  return node;
}

////////////////////
//// files /////////
////////////////////

struct open_file_node {
  int fd;
  struct file* file;
  struct list_elem elem;
};

static bool open_file_eq (const struct list_elem *list_elem, const struct list_elem *_ UNUSED, void *aux) {
  struct open_file_node* node = list_entry (list_elem, struct open_file_node, elem);
  int target = *((int *) aux);

  return node->fd == target;
}

static struct open_file_node* find_open_file (struct process_node * process, int fd) {
  struct list_elem * found = list_find (&process->open_files, open_file_eq, NULL, &fd); 
  if (found == NULL)
    return NULL;

  return list_entry (found, struct open_file_node, elem);
}

int add_process_open_file (struct process_node* node, struct file* file) {
  ASSERT (node != NULL);

  lock_acquire (& node->lock);

  struct open_file_node* open_file = malloc (sizeof (struct open_file_node));
  const int fd = node->fd_counter;
  open_file->fd = fd;
  node->fd_counter++;
  open_file->file = file;
  
  list_push_back (&node->open_files, &open_file->elem);

  lock_release (& node->lock);
  return fd;
}

static void close_file (struct open_file_node* file_node) {
  lock_acquire (&filesys_monitor);
  file_close (file_node->file);
  lock_release (&filesys_monitor);

  free (file_node); 
}

// monitor lock must be held previously
static void close_open_files (struct process_node* node) {
  lock_acquire (&filesys_monitor);
  file_close (node->exec_file);
  lock_release (&filesys_monitor);

  while (!list_empty (&node->open_files))
  {
    struct list_elem *e = list_pop_front (&node->open_files);
    struct open_file_node* file_node = list_entry (e, struct open_file_node, elem); 

    close_file (file_node);
  }
}

bool process_close_file (struct process_node* node, int fd) {
  ASSERT (fd >= 0);
  ASSERT (node != NULL);

  lock_acquire (& node->lock);


  struct open_file_node* file_node = find_open_file (node, fd);
  if (file_node == NULL) {
    lock_release (& node->lock);
    return false;
  }

  list_remove (&file_node->elem);
  close_file (file_node);

  lock_release (& node->lock);
  return true;
}

struct file* get_process_open_file (struct process_node* node, int fd) {
  ASSERT (fd >= 0);
  ASSERT (node != NULL);

  lock_acquire (& node->lock);

  struct open_file_node* file_node = find_open_file (node, fd);
  if (file_node == NULL) {
    lock_release (& node->lock);
    return NULL;
  }
  struct file* file = file_node->file;
  
  lock_release (& node->lock);

  return file;
}

////////////////////
//// exit code /////
////////////////////

static void destroy_vm_page_table(struct process_node* process);


/**
 * Supposed to be called immediately before process exit.
 * Will close all open files.
 */
void process_add_exit_code (struct process_node* node, int exit_code) {
  ASSERT (! intr_context());
  ASSERT (node != NULL);
  
  lock_acquire (& node->lock);

  ASSERT (! node->exited); // this must be called only once

  close_open_files (node);
  destroy_vm_page_table (node);

  node->exited = true;
  node->exit_code = exit_code;
  cond_signal (&node->cond_exited, &node->lock);

  lock_release (& node->lock);

  // print_active_files();
  // print_strings_pool();
  // print_frame_table();
}


int collect_process_exit_code (struct process_node* node) {
  ASSERT (! intr_context());

  if (node == NULL) {
    return BAD_EXIT_CODE;
  }

  lock_acquire (&node->lock);

  const tid_t parent_tid = current_thread_tid ();
  if (parent_tid != node->parent_pid) {
    lock_release (&node->lock);
    return BAD_EXIT_CODE;
  }

  if (!node->exited) {
    cond_wait (&node->cond_exited, &node->lock);
  } 

  const int exit_code = node->exit_code;
  lock_acquire(&processes.lock);
  lock_release (&node->lock);
  remove_process (node);
  lock_release(&processes.lock);

  return exit_code;
}

static void print_exit_code (struct process_node* node) {
  ASSERT (node != NULL);
  lock_acquire (&node->lock);
  
  ASSERT (node->exited);
  
  printf ("%s: exit(%d)\n", node->name, node->exit_code);
  
  lock_release (&node->lock);
}

void exit_curr_process(int exit_code, bool should_print_exit_code) {
  struct process_node* process = find_current_thread_process();
  process_add_exit_code (process, exit_code);
  if (should_print_exit_code) {
    print_exit_code (process);
  }

  thread_exit ();

  NOT_REACHED ();
}

////////////////////
////     VM    /////
////////////////////

struct vm_node {
  struct process_node* process; // parent process's lock
  struct hash_elem hash_elem;
  struct list_elem list_elem; // for frame_table

  struct frame_node* frame;
  uintptr_t page_vaddr;
  struct page_common page_common;
};

static inline bool is_mapped(struct vm_node* node) {
  return node->frame != NULL;
}

static unsigned int vm_table_hash (const struct hash_elem *e, void *_ UNUSED) {
  struct vm_node *node = hash_entry (e, struct vm_node, hash_elem);
  return hash_int (node->page_vaddr);
}

static bool vm_table_less (const struct hash_elem *l, const struct hash_elem *r, void *_ UNUSED) {
  struct vm_node *node_l = hash_entry (l, struct vm_node, hash_elem);
  struct vm_node *node_r = hash_entry (r, struct vm_node, hash_elem);

  return node_l->page_vaddr < node_r->page_vaddr;
}

struct list_elem* get_vm_node_list_elem(struct vm_node* node) {
  ASSERT (node != NULL);
  return &node->list_elem;
}

static struct vm_node* create_vm_node(void) {
  struct vm_node* node = malloc (sizeof (struct vm_node));
  node->page_vaddr = 0;
  node->page_common.type = -1;
  node->process = NULL;
  node->frame = NULL;

  return node;
}

struct vm_node* add_file_backed_vm(struct process_node* process, uint8_t* vaddr, const char* file_path, off_t offset, size_t num_zero_padding, bool readonly, bool exec_file_source) {
  ASSERT (process != NULL);
  ASSERT (! process->exited);
  ASSERT (exec_file_source || !readonly);
  ASSERT (is_user_vaddr (vaddr));
  ASSERT (is_page_aligned (vaddr))
  ASSERT (num_zero_padding <= PGSIZE);

  lock_acquire (&process->lock);

  struct vm_node* node = create_vm_node();
  if (node == NULL) {
    lock_release (&process->lock);
    return NULL;
  }

  struct file_page_node* file_page = create_file_page_node(file_path, offset, num_zero_padding);
  if (file_page == NULL) {
    free (node);
    lock_release (&process->lock);
    return NULL;
  }

  node->page_vaddr = (uintptr_t) vaddr;
  node->process = process;

  const bool shared_executable = readonly && exec_file_source;
  const bool file_backed_executable = !readonly && exec_file_source;
  const bool file_backed = !readonly && !exec_file_source;

  if (shared_executable) {
    struct file_offset_mapping* shared_executable = add_active_file(file_page);
    if (shared_executable == NULL) {
      free (node);
      destroy_file_page_node(file_page);
      lock_release (&process->lock);
      return NULL;
    }
    node->page_common = init_shared_executable(shared_executable);
  } else if (file_backed_executable) {
    node->page_common = init_file_backed_executable(file_page);
  } else if (file_backed) {
    node->page_common = init_file_backed(file_page);
  } else {
    NOT_REACHED();
  }

  ASSERT (hash_insert(&process->vm_table, &node->hash_elem) == NULL);

  lock_release (&process->lock);

  return node;
}

static void destroy_vm_page_node (struct vm_node *node) {

  ASSERT (node != NULL);
  ASSERT (lock_held_by_current_thread(&node->process->lock));

  enum page_source_type type = node->page_common.type;

  if (is_mapped (node)) {
    destroy_frame_lockable(node->frame, false);
  } else {
    if (type == FILE_BACKED_EXECUTABLE && node->page_common.body.file_backed_executable.file_loaded) { // in swap
      PANIC("NOT IMPLEMENTED");
    } else if (type == FREESTANDING) {
      PANIC("NOT IMPLEMENTED");
    }
  }

  switch (node->page_common.type) {
    case SHARED_EXECUTABLE:
      destroy_active_file(node->page_common.body.shared_executable);
      break;
    case FILE_BACKED:
      PANIC("NOT_IMPL");
      break;
    case FILE_BACKED_EXECUTABLE:
      destroy_file_page_node(node->page_common.body.file_backed_executable.file);
      break;
    case FREESTANDING:
      PANIC("NOT_IMPL");
      break;
    default:
      NOT_REACHED();
  }

  free (node);
}

/* Adds a mapping from user virtual address UPAGE to kernel 
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool install_page (uint32_t* pagedir, void *upage, void *kpage, bool writable)
{
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (pagedir, upage) == NULL
          && pagedir_set_page (pagedir, upage, kpage, writable));
}

void* activate_vm_page(struct vm_node* node) {
  ASSERT (node != NULL);
  ASSERT (node->frame == NULL);

  lock_acquire (&node->process->lock);

  switch (node->page_common.type) {
    case SHARED_EXECUTABLE:
      node->frame = load_file_offset_mapping_page(node->page_common.body.shared_executable);
      break;
    case FILE_BACKED:
      PANIC("NOT_IMPL");
      break;
    case FILE_BACKED_EXECUTABLE:
      if (! node->page_common.body.file_backed_executable.file_loaded) {
        node->frame = load_file_page_frame(node->page_common.body.file_backed_executable.file);
        if (node->frame != NULL) {
          node->page_common.body.file_backed_executable.file_loaded = true;
        }
      } else {
        // TODO load swap
        PANIC("NOT IMPL");
      }
      break;
    case FREESTANDING:
      PANIC("NOT_IMPL");
      break;
    default:
      NOT_REACHED();
  }

  // failed to load
  if (node->frame == NULL) {
    lock_release (&node->process->lock);
    return NULL;
  }

  add_frame_vm_page(node->frame, node, &node->page_common);
  void* paddr = get_frame_phys_addr(node->frame);
  void* vaddr = (void*) node->page_vaddr;
  const bool writable = ! is_page_common_readonly(&node->page_common);
  if (! install_page(node->process->pagedir, vaddr, paddr, writable)) {
    struct lock* lock = &node->process->lock;
    destroy_vm_page_node(node);
    lock_release (lock);
    return NULL;
  }

  lock_release (&node->process->lock);

  return paddr;
}


static void destroy_vm_page (struct hash_elem *e, void *_ UNUSED) {

  // lock should be hel by here
  ASSERT (e != NULL);

  struct vm_node *node = hash_entry (e, struct vm_node, hash_elem);
  destroy_vm_page_node(node);
}

static void vm_node_print (struct hash_elem *e, void *_ UNUSED) {
  struct vm_node *node = hash_entry (e, struct vm_node, hash_elem);

  printf("(mapped=%d, page_nr=%x body=", node->frame != NULL, node->page_vaddr);
  print_page_common(&node->page_common);
  printf(")\n");
}

void print_process_vm(struct process_node* process) {
  if (process == NULL) {
    printf ("Process not found");
    return;
  }

  lock_acquire (&process->lock);

  printf ("Process (name=%s, pid=%d, len=%lu): \n", process->name, process->pid, hash_size(&process->vm_table));
  hash_apply (&process->vm_table, vm_node_print);
  printf("---\n");

  lock_release (&process->lock);
}

static void destroy_vm_page_table(struct process_node* process) {
  hash_destroy (&process->vm_table, destroy_vm_page);
}

void deactivate_vm_node_list(struct list* list, bool lockable) {
  for (struct list_elem *e = list_begin (list); e != list_end (list); e = list_next (e)) {
    struct vm_node* node = list_entry(e, struct vm_node, list_elem);

    if (lockable) 
      lock_acquire (&node->process->lock);

    pagedir_clear_page (node->process->pagedir, (void*) node->page_vaddr); // unmap from pagedir
    node->frame = NULL;

    if (lockable)
      lock_release (&node->process->lock);
  }

}