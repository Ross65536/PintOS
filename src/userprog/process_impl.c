#include "process.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#include <debug.h>
#include <string.h>
#include <stdio.h>

struct process_node {
  char name[PROCESS_MAX_NAME];
  tid_t parent_tid;
  tid_t tid;

  // exit codes
  struct condition cond_exited;
  bool exited;
  int exit_code;

  struct list_elem elem;
};

struct process_exit_codes {
  struct list processes;
  struct lock monitor_lock;
} process_exit_codes;

void 
process_impl_init () {
  list_init (&process_exit_codes.processes);
  lock_init (&process_exit_codes.monitor_lock);
}

static bool process_eq (const struct list_elem *list_elem, const struct list_elem *_ UNUSED, void *aux) {
  struct process_node* node = list_entry (list_elem, struct process_node, elem);
  tid_t target = *((tid_t *) aux);

  return node->tid == target;
}

static struct process_node* find_process (tid_t tid) {
  struct list_elem * found = list_find (&process_exit_codes.processes, process_eq, NULL, &tid); 
  if (found == NULL)
    return NULL;

  return list_entry (found, struct process_node, elem);
}

static void remove_process (struct process_node* node) {
  list_remove (&node->elem);
  free (node);
}

void add_process (tid_t parent_tid, tid_t tid, const char* name) {
  ASSERT (! intr_context());

  lock_acquire (& process_exit_codes.monitor_lock);

  if (find_process (tid) != NULL) {
    lock_release (& process_exit_codes.monitor_lock);
    return;
  }

  struct process_node* node = malloc (sizeof (struct process_node));
  strlcpy (node->name, name, PROCESS_MAX_NAME);
  node->parent_tid = parent_tid;
  node->tid = tid;
  node->exited = false;
  node->exit_code = BAD_EXIT_CODE;
  cond_init(&node->cond_exited);

  list_push_back (& process_exit_codes.processes, &node->elem);

  lock_release (& process_exit_codes.monitor_lock);
}

void process_add_exit_code (tid_t tid, int exit_code) {
  ASSERT (! intr_context());
  
  lock_acquire (& process_exit_codes.monitor_lock);

  struct process_node* node = find_process (tid);
  ASSERT (node != NULL);

  node->exited = true;
  node->exit_code = exit_code;
  cond_signal (&node->cond_exited, &process_exit_codes.monitor_lock);

  lock_release (& process_exit_codes.monitor_lock);
}

int collect_process_exit_code (tid_t tid) {
  ASSERT (! intr_context());

  lock_acquire (&process_exit_codes.monitor_lock);

  struct process_node* node = find_process (tid);
  tid_t parent_tid = thread_current ()->tid;
  if (node == NULL || parent_tid != node->parent_tid) {
    lock_release (&process_exit_codes.monitor_lock);
    return BAD_EXIT_CODE;
  }

  if (!node->exited) {
    cond_wait (&node->cond_exited, &process_exit_codes.monitor_lock);
  }

  const int exit_code = node->exit_code;
  remove_process (node);
  
  lock_release (&process_exit_codes.monitor_lock);

  return exit_code;
}

static void print_exit_code (tid_t tid) {
  lock_acquire (&process_exit_codes.monitor_lock);

  struct process_node* node = find_process (tid);
  ASSERT (node != NULL);
  ASSERT (node->exited);
  
  printf ("%s: exit(%d)\n", node->name, node->exit_code);
  
  lock_release (&process_exit_codes.monitor_lock);
}

void exit_curr_process(int exit_code, bool should_print_exit_code) {
  tid_t tid = thread_current ()->tid;
  process_add_exit_code (tid, exit_code);
  if (should_print_exit_code) {
    print_exit_code (tid);
  }

  thread_exit ();

  NOT_REACHED ();
}

