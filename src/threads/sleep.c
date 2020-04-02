#include <inttypes.h>
#include <kernel/list.h>
#include <debug.h>

#include "threads/sleep.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "devices/timer.h"

struct sleep_node
{
  struct list_elem list_elem;
  int64_t target_tick;
  struct condition waiter;
};


static struct sleep_queue {
  struct list sorted_threads;
  struct lock monitor_lock;
} sleeping_threads;

void
sleep_init (void) {
  list_init (&sleeping_threads.sorted_threads);
  lock_init (&sleeping_threads.monitor_lock);
}

static bool
sleep_node_list_less_func (const struct list_elem *left_e, const struct list_elem *right_e, void *_aux UNUSED) 
{
  struct sleep_node* left = list_entry(left_e, struct sleep_node, list_elem);
  struct sleep_node* right = list_entry(right_e, struct sleep_node, list_elem);

  return left->target_tick < right->target_tick;
}

void 
sleep_curr_thread (int64_t target_tick) 
{
  ASSERT (!intr_context ());
  
  struct sleep_node node;
  node.target_tick = target_tick;
  cond_init(&node.waiter); 

  lock_acquire (&sleeping_threads.monitor_lock);
  list_insert_ordered (&sleeping_threads.sorted_threads, &node.list_elem, sleep_node_list_less_func, NULL);

  cond_wait(&node.waiter, &sleeping_threads.monitor_lock); // sleep

  lock_release (&sleeping_threads.monitor_lock);
  ASSERT (cond_no_waiters(&node.waiter));
}

void 
thread_sleep_tick ()
{
  ASSERT (intr_context ());

  const int64_t curr_ticks = timer_ticks ();
  if (! lock_try_acquire (&sleeping_threads.monitor_lock))
    return;

  for (struct list_elem *e = list_begin (&sleeping_threads.sorted_threads); e != list_end (&sleeping_threads.sorted_threads); )
  {
    struct sleep_node *node = list_entry (e, struct sleep_node, list_elem); 
    if (node->target_tick <= curr_ticks) {
      ASSERT (! cond_no_waiters (&node->waiter));
      
      e = list_remove (e);
      cond_signal_with_yielding (&node->waiter, &sleeping_threads.monitor_lock, false);
    } else {
      break;
    }
  } 

  lock_release_with_locking (&sleeping_threads.monitor_lock, false);
}