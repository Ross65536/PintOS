#include "process_exit.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/interrupt.h"

struct process_exit_node {
  tid_t parent_tid;
  tid_t tid;
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
process_exit_init () {
  list_init (&process_exit_codes.processes);
  lock_init (&process_exit_codes.monitor_lock);
}

static bool process_exit_eq (const struct list_elem *list_elem, const struct list_elem *_ UNUSED, void *aux) {
  struct process_exit_node* node = list_entry (list_elem, struct process_exit_node, elem);
  tid_t target = *((tid_t *) aux);

  return node->tid == target;
}

static struct process_exit_node* find_process (tid_t tid) {
  struct list_elem * found = list_find (&process_exit_codes.processes, process_exit_eq, NULL, &tid); 
  if (found == NULL)
    return NULL;

  return list_entry (found, struct process_exit_node, elem);
}

// must be called with monitor lock held
static struct process_exit_node* insert_process(tid_t parent_tid, tid_t tid) {
  struct process_exit_node* node = malloc (sizeof (struct process_exit_node));
  node->parent_tid = parent_tid;
  node->tid = tid;
  node->exited = false;
  node->exit_code = -1;
  cond_init(&node->cond_exited);

  list_push_back (& process_exit_codes.processes, &node->elem);

  return node;
}

static void remove_process (struct process_exit_node* node) {
  list_remove (&node->elem);
  free (node);
}

void add_process_exit_elem (tid_t parent_tid, tid_t tid) {
  ASSERT (! intr_context());

  lock_acquire (& process_exit_codes.monitor_lock);

  ASSERT((find_process (tid) == NULL));

  insert_process (parent_tid, tid);

  lock_release (& process_exit_codes.monitor_lock);
}

void process_add_exit_code (tid_t tid, int exit_code) {
  ASSERT (! intr_context());
  
  lock_acquire (& process_exit_codes.monitor_lock);

  struct process_exit_node* node = find_process (tid);
  ASSERT (node != NULL);

  node->exited = true;
  node->exit_code = exit_code;
  cond_signal (&node->cond_exited, &process_exit_codes.monitor_lock);

  lock_release (& process_exit_codes.monitor_lock);
}

int collect_process_exit_code (tid_t tid) {
  ASSERT (! intr_context());

  lock_acquire (&process_exit_codes.monitor_lock);

  struct process_exit_node* node = find_process (tid);
  tid_t parent_tid = thread_current ()->tid;
  if (node == NULL || parent_tid != node->parent_tid) {
    return -1;
  }

  if (!node->exited) {
    cond_wait (&node->cond_exited, &process_exit_codes.monitor_lock);
  }

  const int exit_code = node->exit_code;
  remove_process (node);
  
  lock_release (&process_exit_codes.monitor_lock);

  return exit_code;
}