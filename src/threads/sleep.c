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
  struct semaphore waiter;
};


static struct sleep_queue {
  struct list sorted_threads;
  struct lock list_lock;
} sleeping_threads;

void
sleep_init (void) {
  list_init (&sleeping_threads.sorted_threads);
  lock_init (&sleeping_threads.list_lock);
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
  sema_init(&node.waiter, 0);

  lock_acquire (&sleeping_threads.list_lock);
  list_insert_ordered (&sleeping_threads.sorted_threads, &node.list_elem, sleep_node_list_less_func, NULL);
  lock_release (&sleeping_threads.list_lock);

  sema_down(&node.waiter); // sleep
  ASSERT (sema_no_waiters(&node.waiter));
}

void 
thread_sleep_tick ()
{
  // ASSERT (! intr_context ());
  const int64_t curr_ticks = timer_ticks ();

  enum intr_level old_level = intr_disable ();
  for (struct list_elem *e = list_begin (&sleeping_threads.sorted_threads); e != list_end (&sleeping_threads.sorted_threads); )
  {
    struct sleep_node *node = list_entry (e, struct sleep_node, list_elem); 
    if (node->target_tick <= curr_ticks) {
      ASSERT (! sema_no_waiters (&node->waiter));
      
      e = list_remove (e);
      sema_up (&node->waiter);
      
    } else {
      break;
    }
  }
  intr_set_level (old_level);

}