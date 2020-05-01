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

struct lock filesys_monitor;

struct process_node {
  struct hash_elem elem;
  char name[PROCESS_MAX_NAME];
  tid_t parent_pid;
  tid_t pid; // the process pid, same as thread tid

  // exit codes
  struct condition cond_exited;
  bool exited;
  int exit_code;

  // files
  int fd_counter;
  struct list open_files;
  struct file* exec_file;

};

struct open_file_node {
  int fd;
  struct file* file;
  struct list_elem elem;
};

struct processes {
  struct hash processes;
  struct lock monitor_lock;
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
  lock_init (&processes.monitor_lock);
  lock_init (&filesys_monitor);
}

static struct process_node* find_process (tid_t tid) {
  struct process_node node_find;
  node_find.pid = tid;
  struct hash_elem * found = hash_find (&processes.processes, &node_find.elem); 
  if (found == NULL)
    return NULL;

  return hash_entry (found, struct process_node, elem);
}

static void remove_process (struct process_node* node) {
  hash_delete (&processes.processes, &node->elem);
  free (node);
}

void add_process (tid_t parent_tid, tid_t tid, const char* name, struct file* exec_file) {
  ASSERT (! intr_context());

  lock_acquire (& processes.monitor_lock);

  if (find_process (tid) != NULL) {
    lock_release (& processes.monitor_lock);
    return;
  }

  struct process_node* node = malloc (sizeof (struct process_node));
  strlcpy (node->name, name, PROCESS_MAX_NAME);
  node->parent_pid = parent_tid;
  node->pid = tid;
  node->exited = false;
  node->exit_code = BAD_EXIT_CODE;
  cond_init(&node->cond_exited);
  list_init (&node->open_files);
  node->fd_counter = 2;
  node->exec_file = exec_file;

  hash_insert (& processes.processes, &node->elem);

  lock_release (& processes.monitor_lock);
}

////////////////////
//// files /////////
////////////////////

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

int add_process_open_file (tid_t tid, struct file* file) {
  lock_acquire (& processes.monitor_lock);

  struct process_node* node = find_process (tid);
  ASSERT (node != NULL);

  struct open_file_node* open_file = malloc (sizeof (struct open_file_node));
  const int fd = node->fd_counter;
  open_file->fd = fd;
  node->fd_counter++;
  open_file->file = file;
  
  list_push_back (&node->open_files, &open_file->elem);

  lock_release (& processes.monitor_lock);
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

bool process_close_file (tid_t tid, int fd) {
  ASSERT (fd >= 0);

  lock_acquire (& processes.monitor_lock);

  struct process_node* node = find_process (tid);
  ASSERT (node != NULL);

  struct open_file_node* file_node = find_open_file (node, fd);
  if (file_node == NULL) {
    lock_release (& processes.monitor_lock);
    return false;
  }

  list_remove (&file_node->elem);
  close_file (file_node);

  lock_release (& processes.monitor_lock);
  return true;
}

struct file* get_process_open_file (tid_t tid, int fd) {
  ASSERT (fd >= 0);

  lock_acquire (& processes.monitor_lock);

  struct process_node* node = find_process (tid);
  ASSERT (node != NULL);

  struct open_file_node* file_node = find_open_file (node, fd);
  if (file_node == NULL) {
    lock_release (& processes.monitor_lock);
    return NULL;
  }
  struct file* file = file_node->file;
  
  lock_release (& processes.monitor_lock);

  return file;
}

////////////////////
//// exit code /////
////////////////////

/**
 * Supposed to be called immediately before process exit.
 * Will close all open files.
 */
void process_add_exit_code (tid_t tid, int exit_code) {
  ASSERT (! intr_context());
  
  lock_acquire (& processes.monitor_lock);

  struct process_node* node = find_process (tid);
  ASSERT (node != NULL);
  ASSERT (! node->exited); // this must be called only once

  close_open_files (node);

  node->exited = true;
  node->exit_code = exit_code;
  cond_signal (&node->cond_exited, &processes.monitor_lock);

  lock_release (& processes.monitor_lock);
}

int collect_process_exit_code (tid_t tid) {
  ASSERT (! intr_context());

  lock_acquire (&processes.monitor_lock);

  struct process_node* node = find_process (tid);
  tid_t parent_tid = current_thread_tid ();
  if (node == NULL || parent_tid != node->parent_pid) {
    lock_release (&processes.monitor_lock);
    return BAD_EXIT_CODE;
  }

  if (!node->exited) {
    cond_wait (&node->cond_exited, &processes.monitor_lock);
  }

  const int exit_code = node->exit_code;
  remove_process (node);
  
  lock_release (&processes.monitor_lock);

  return exit_code;
}

void print_exit_code (tid_t tid) {
  lock_acquire (&processes.monitor_lock);

  struct process_node* node = find_process (tid);
  ASSERT (node != NULL);
  ASSERT (node->exited);
  
  printf ("%s: exit(%d)\n", node->name, node->exit_code);
  
  lock_release (&processes.monitor_lock);
}

void exit_curr_process(int exit_code, bool should_print_exit_code) {
  tid_t tid = current_thread_tid ();
  process_add_exit_code (tid, exit_code);
  if (should_print_exit_code) {
    print_exit_code (tid);
  }

  thread_exit ();

  NOT_REACHED ();
}

