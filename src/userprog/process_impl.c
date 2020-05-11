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
#include "filesys/filesys.h"
#include "process_vm.h"
#include "vm/frame_table.h"
#include "vm/file_page.h"
#include "vm/active_files.h"
#include "vm/page_common.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "process_impl.h"
#include "vm/strings_pool.h"
#include "userprog/vm.h"

struct lock filesys_monitor;

struct process_node {
  struct lock lock;

  struct hash_elem elem;
  const char* name;
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
  void* syscall_stack_ptr;

  // file mmap
  int mapid_counter;
  struct list file_mmaps;
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
  remove_string_pool(node->name);
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

  node->name = add_string_pool(name);
  node->parent_pid = parent_tid;
  node->pid = tid;
  node->exited = false;
  node->exit_code = BAD_EXIT_CODE;
  node->pagedir = pagedir;
  cond_init(&node->cond_exited);
  list_init (&node->open_files);
  node->fd_counter = 2;
  node->exec_file = exec_file;
  node->syscall_stack_ptr = NULL;
  list_init(&node->file_mmaps);
  node->mapid_counter = 0;
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
  const char* file_path;
};

inline static void print_open_file_node(struct open_file_node* file) {
  printf("(fd=%d, path=%s", file->fd, file->file_path);
}

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

int add_process_open_file (struct process_node* node, const char* file_path) {
  ASSERT (node != NULL);

  lock_acquire (&filesys_monitor);
  struct file * file = filesys_open (file_path);
  lock_release (&filesys_monitor);

  if (file == NULL) 
    return BAD_EXIT_CODE;

  lock_acquire (& node->lock);

  struct open_file_node* open_file = malloc (sizeof (struct open_file_node));
  const int fd = node->fd_counter;
  open_file->fd = fd;
  node->fd_counter++;
  open_file->file = file;
  open_file->file_path = add_string_pool(file_path);
  
  list_push_back (&node->open_files, &open_file->elem);

  lock_release (& node->lock);
  return fd;
}

static void close_file (struct open_file_node* file_node) {
  lock_acquire (&filesys_monitor);
  file_close (file_node->file);
  lock_release (&filesys_monitor);

  remove_string_pool(file_node->file_path);
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
void destroy_mmaps(struct process_node* process);

/**
 * Supposed to be called immediately before process exit.
 * Will close all open files.
 */
void process_add_exit_code (struct process_node* node, int exit_code) {
  ASSERT (! intr_context());
  ASSERT (node != NULL);

  // print_active_files(&readonly_files);
  // print_active_files(&writable_files);
  
  lock_acquire (& node->lock);

  ASSERT (! node->exited); // this must be called only once

  destroy_mmaps(node);
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

static struct vm_node* create_vm_node(void * vaddr, struct process_node* process) {
  struct vm_node* node = malloc (sizeof (struct vm_node));
  node->page_vaddr = (uintptr_t) vaddr;
  node->process = process;

  node->page_common.type = -1;
  node->frame = NULL;

  return node;
}

static struct vm_node* add_file_backed_vm_internal(struct process_node* process, uint8_t* vaddr, const char* file_path, off_t offset, size_t num_zero_padding, bool readonly, bool exec_file_source) {
  ASSERT (process != NULL);
  ASSERT (! process->exited);
  ASSERT (is_user_vaddr (vaddr));
  ASSERT (is_page_aligned (vaddr))
  ASSERT (num_zero_padding <= PGSIZE);
  ASSERT (lock_held_by_current_thread(&process->lock));

  struct vm_node* node = create_vm_node(vaddr, process);
  if (node == NULL) {
    return NULL;
  }

  struct file_page_node* file_page = create_file_page_node(file_path, offset, num_zero_padding);
  if (file_page == NULL) {
    free (node);
    return NULL;
  }

  const bool shared_readonly_file = readonly;
  const bool shared_writable_file = !readonly && !exec_file_source;
  const bool file_backed_executable_static = !readonly && exec_file_source;

  if (shared_readonly_file || shared_writable_file) {
    struct active_files_list* active_files = shared_readonly_file ? &readonly_files : &writable_files;
    struct file_offset_mapping* shared_executable = add_active_file(active_files, file_page);
    if (shared_executable == NULL) {
      free (node);
      destroy_file_page_node(file_page);
      return NULL;
    }
    node->page_common = shared_readonly_file ? 
        init_shared_readonly_file_backed(shared_executable) :
        init_shared_writable_file_backed(shared_executable);
  } else if (file_backed_executable_static) {
    node->page_common = init_file_backed_executable_static(file_page);
  } else {
    NOT_REACHED();
  }
 
  ASSERT (hash_insert(&process->vm_table, &node->hash_elem) == NULL);

  return node;
}

struct vm_node* add_file_backed_vm(struct process_node* process, uint8_t* vaddr, const char* file_path, off_t offset, size_t num_zero_padding, bool readonly, bool exec_file_source) {
  ASSERT (process != NULL);
  ASSERT (! process->exited);
  ASSERT (! (!readonly && !exec_file_source));

  lock_acquire (&process->lock);

  struct vm_node* node = add_file_backed_vm_internal(process, vaddr, file_path, offset, num_zero_padding, readonly, exec_file_source);

  lock_release (&process->lock);

  return node;
}

struct vm_node* add_stack_freestanding_vm(struct process_node* process, uint8_t* vaddr) {
  ASSERT (process != NULL);
  ASSERT (! process->exited);
  ASSERT (is_user_vaddr (vaddr));
  ASSERT (is_page_aligned (vaddr));

  lock_acquire (&process->lock);

  struct vm_node* node = create_vm_node(vaddr, process);
  if (node == NULL) {
    lock_release (&process->lock);
    return NULL;
  }

  node->page_common = init_freestanding();

  ASSERT (hash_insert(&process->vm_table, &node->hash_elem) == NULL);

  lock_release (&process->lock);

  return node;
}



static void unmap_vm_node(struct vm_node* node) {
  ASSERT (lock_held_by_current_thread(&node->process->lock));
  pagedir_clear_page (node->process->pagedir, (void*) node->page_vaddr); // unmap from pagedir
  node->frame = NULL;
}

static void destroy_vm_page_node (struct vm_node *node) {

  ASSERT (node != NULL);
  ASSERT (lock_held_by_current_thread(&node->process->lock));

  enum page_source_type type = node->page_common.type;

  if (is_mapped (node)) {
    struct frame_node* frame = node->frame; 
    if (is_page_common_shared_file(&node->page_common)) {
      remove_frame_vm_node(frame, &node->frame_list_elem);
      unmap_vm_node(node);
    } else {
      destroy_frame(frame);
    }
  } else {
    if (type == FILE_BACKED_EXECUTABLE_STATIC && node->page_common.body.file_backed_executable_static.swap.is_swapped) { // in swap
      PANIC("NOT IMPLEMENTED");
    } else if (type == FREESTANDING) {
      PANIC("NOT IMPLEMENTED");
    }
  }

  switch (node->page_common.type) {
    case SHARED_READONLY_FILE:
    case SHARED_WRITABLE_FILE: {
      const bool shared_readonly = node->page_common.type == SHARED_READONLY_FILE;
      struct active_files_list* active_files = shared_readonly ? &readonly_files : &writable_files;
      struct file_offset_mapping* file = shared_readonly ? node->page_common.body.shared_readonly_file : node->page_common.body.shared_writable_file;
      destroy_active_file(active_files, file);
      break;
    }
    case FILE_BACKED_EXECUTABLE_STATIC:
      destroy_file_page_node(node->page_common.body.file_backed_executable_static.file);
      break;
    case FREESTANDING:
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

  // print_process_vm(node->process);
  ASSERT (node != NULL);
  ASSERT (! is_mapped(node));

  lock_acquire (&node->process->lock);

  switch (node->page_common.type) {
    case SHARED_READONLY_FILE:
    case SHARED_WRITABLE_FILE:
      node->frame = load_file_offset_mapping_page(get_page_common_shared_active_file(&node->page_common));
      break;
    case FILE_BACKED_EXECUTABLE_STATIC:
      if (node->page_common.body.file_backed_executable_static.swap.is_swapped) {
        PANIC("NOT IMPL");
      } else {
        node->frame = load_file_page_frame(node->page_common.body.file_backed_executable_static.file);
      }
      break;
    case FREESTANDING:
      if (node->page_common.body.freestanding.is_swapped) {
        PANIC("NOT IMPL");
      } else {
        node->frame = allocate_user_page();
      }
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
    destroy_frame(node->frame);
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

static void print_vm_node (struct vm_node *node) {
  ASSERT (lock_held_by_current_thread(&node->process->lock));

  printf("(mapped=%d, page_nr=%x body=", node->frame != NULL, node->page_vaddr);
  print_page_common(&node->page_common);
}

static void vm_node_print (struct hash_elem *e, void *_ UNUSED) {
  struct vm_node *node = hash_entry (e, struct vm_node, hash_elem);

  print_vm_node (node);
  printf(")\n");
}

void print_process_vm(struct process_node* process) {
  if (process == NULL) {
    printf ("Process not found");
    return;
  }

  lock_acquire (&process->lock);

  printf ("Process VM (name=%s, pid=%d, len=%lu): \n", process->name, process->pid, hash_size(&process->vm_table));
  hash_apply (&process->vm_table, vm_node_print);
  printf("---\n");

  lock_release (&process->lock);
}

static void destroy_vm_page_table(struct process_node* process) {
  hash_destroy (&process->vm_table, destroy_vm_page);
}

bool is_vm_node_dirty(struct vm_node* node) {
  ASSERT (node != NULL);
  ASSERT (is_mapped (node));

  const bool held = lock_acquire_if_not_held (&node->process->lock);
  if (!is_mapped(node)) {
    lock_release_if_not_held (&node->process->lock, held);
    return false;
  }
  
  const bool dirty = pagedir_is_dirty(node->process->pagedir, (void*) node->page_vaddr);
  lock_release_if_not_held (&node->process->lock, held);

  return dirty;
}

void unmap_vm_node_frame(struct vm_node* node) {
  const bool held = lock_acquire_if_not_held (&node->process->lock);
  if (is_mapped(node))
    unmap_vm_node(node);
  lock_release_if_not_held (&node->process->lock, held);
}

static struct vm_node* find_vm_node_internal(struct process_node* process, void* address) {
  ASSERT (lock_held_by_current_thread(&process->lock));
  ASSERT (is_page_aligned (address));

  struct vm_node node;
  node.page_vaddr = (uintptr_t) address;

  struct hash_elem* e = hash_find(&process->vm_table, &node.hash_elem);

  if (e == NULL) {
    return NULL;
  }

  return hash_entry(e, struct vm_node, hash_elem);
}

struct vm_node* find_vm_node(struct process_node* process, void* address) {
  ASSERT (process != NULL);

  lock_acquire (&process->lock);

  struct vm_node* node = find_vm_node_internal(process, address);
  lock_release (&process->lock);

  return node;
}

void add_process_user_stack_ptr(struct process_node* process, void* address) {
  ASSERT (process != NULL);

  lock_acquire (&process->lock);

  process->syscall_stack_ptr = address;

  lock_release (&process->lock);
}

void* collect_process_user_stack_ptr(struct process_node* process) {
  ASSERT (process != NULL);

  lock_acquire (&process->lock);
  void* addr = process->syscall_stack_ptr;
  ASSERT(addr != NULL);

  lock_release (&process->lock);

  return addr;
}


////////////////////
////  MMAP     /////
////////////////////

struct mmap_node {
  int mapid;
  struct list_elem elem;
  struct list vm_nodes;
};

int add_file_mapping(struct process_node* process, int fd, void* addr) {
  ASSERT (process != NULL);
  ASSERT (fd >= 2);
  ASSERT (is_page_aligned(addr) && addr != NULL);

  lock_acquire (&process->lock);

  struct open_file_node* file_node = find_open_file(process, fd);
  if (file_node == NULL) {
    lock_release (&process->lock);
    return MMAP_ERROR;
  }

  lock_acquire (&filesys_monitor);
  off_t file_size = file_length(file_node->file);
  lock_release (&filesys_monitor);

  if (file_size == 0) {
    lock_release (&process->lock);
    return MMAP_ERROR;
  }

  for (int i = 0; i < file_size; i += PGSIZE) {
    void* page_addr = increment_ptr(addr, i);

    if (find_vm_node_internal(process, page_addr) != NULL) {
      lock_release (&process->lock);
      return MMAP_ERROR;
    }
  }

  struct mmap_node* map_node = malloc (sizeof (struct mmap_node));
  map_node->mapid = process->mapid_counter;
  process->mapid_counter++;
  list_init(&map_node->vm_nodes);

  for (off_t i = 0; i < file_size; i += PGSIZE) {

    void* page_addr = increment_ptr(addr, i);
    struct file_page_node file_page_finder = create_finder (file_node->file_path, i);

    const bool is_last = i + PGSIZE >= file_size;
    size_t num_zeroes = is_last ? PGSIZE - (file_size % PGSIZE) : 0;
    const bool readonly = active_file_exists(&readonly_files, &file_page_finder);
    struct vm_node* vm_node = add_file_backed_vm_internal(process, page_addr, file_node->file_path, i, num_zeroes, readonly, readonly);
    ASSERT (vm_node != NULL); // TODO add destruction

    list_push_back(&map_node->vm_nodes, &vm_node->mmap_list_elem);
  }

  list_push_back(&process->file_mmaps, &map_node->elem);

  lock_release (&process->lock);

  return map_node->mapid;
}


static bool mmap_node_eq (const struct list_elem *list_elem, const struct list_elem *_ UNUSED, void *aux) {
  struct mmap_node* node = list_entry (list_elem, struct mmap_node, elem);
  int target = *((int *) aux);

  return node->mapid == target;
}


static struct mmap_node* find_mmap_node(struct process_node* process, int mmapid) {
  ASSERT (lock_held_by_current_thread(&process->lock));

  struct list_elem * found = list_find (&process->file_mmaps, mmap_node_eq, NULL, &mmapid); 
  if (found == NULL)
    return NULL;

  return list_entry (found, struct mmap_node, elem);
}

static void destroy_mmap_node (struct process_node* process, struct mmap_node* mmap_node) {
  for (struct list_elem *e = list_begin (&mmap_node->vm_nodes); e != list_end (&mmap_node->vm_nodes);) {
    struct vm_node* vm = list_entry (e, struct vm_node, mmap_list_elem);
    e = list_remove (e);
    hash_delete(&process->vm_table, &vm->hash_elem);
    destroy_vm_page_node(vm);
  } 

  free (mmap_node);
}


bool unmap_file_mapping(struct process_node* process, int mmapid) {
  ASSERT (process != NULL);

  lock_acquire (&process->lock);

  struct mmap_node* mmap_node = find_mmap_node (process, mmapid);
  if (mmap_node == NULL) {
    lock_release (&process->lock);
    return false;  
  }

  list_remove(&mmap_node->elem);
  destroy_mmap_node(process, mmap_node);

  lock_release (&process->lock);

  return true;
}

void destroy_mmaps(struct process_node* process) {
  ASSERT (lock_held_by_current_thread(&process->lock));

  for (struct list_elem *e = list_begin (&process->file_mmaps); e != list_end (&process->file_mmaps); ) {
    struct mmap_node* mmap = list_entry (e, struct mmap_node, elem);
    e = list_remove (e);
    destroy_mmap_node(process, mmap);
  }

}

void print_process_mmaps(struct process_node* process) {
   ASSERT (process != NULL);

  lock_acquire (&process->lock);

  printf ("Process MMAPs (name=%s, pid=%d, len=%lu): ", process->name, process->pid, list_size(&process->file_mmaps));

  for (struct list_elem *e = list_begin (&process->file_mmaps); e != list_end (&process->file_mmaps); e = list_next (e)) {
    struct mmap_node* mmap = list_entry (e, struct mmap_node, elem);
    printf ("\n  (mapid=%d, num_vms=%lu) :: ", mmap->mapid, list_size(&mmap->vm_nodes));

    for (struct list_elem *vm_e = list_begin (&mmap->vm_nodes); vm_e != list_end (&mmap->vm_nodes); vm_e = list_next (vm_e)) {
      struct vm_node* vm = list_entry (vm_e, struct vm_node, mmap_list_elem);
      print_vm_node(vm);
      printf (" | ");
    } 

  }

  printf ("\n---\n");

  lock_release (&process->lock);
}