#include "thread.h"
#include "mlfq-scheduler.h"
#include "interrupt.h"

static int mlfq_cmp_thread_priority (struct thread* left_t, struct thread* right_t) {
  return left_t->priority - right_t->priority;
}

static bool mlfq_thread_priority_less (const struct list_elem *left, const struct list_elem *right, void *_ UNUSED) {
  struct thread* left_t = list_entry (left, struct thread, elem);
  struct thread* right_t = list_entry (right, struct thread, elem);

  return mlfq_cmp_thread_priority (left_t, right_t) < 0;
}

struct thread * mlfq_pop_highest_priority_thread (struct list* thread_list) {
  struct list_elem * next_thread = list_pop_max (thread_list, mlfq_thread_priority_less, NULL);

  return list_entry (next_thread, struct thread, elem);
}

/** Should be followed by call to thread_yield()
 */
bool mlfq_should_curr_thread_yield_priority (struct thread * other) { 
  return !intr_context () && mlfq_cmp_thread_priority (other, thread_current ()) > 0;
}

static bool mlfq_cond_waiter_less (const struct list_elem *left, const struct list_elem *right, void *_ UNUSED) {
  struct semaphore * left_sema = & (list_entry (left, struct semaphore_elem, elem)->semaphore);
  struct semaphore * right_sema = & (list_entry (right, struct semaphore_elem, elem)->semaphore);

  ASSERT (!sema_no_waiters (left_sema));
  ASSERT (!sema_no_waiters (right_sema)); 

  struct thread * left_thread = list_entry (list_front (&left_sema->waiters), struct thread, elem);
  struct thread * right_thread = list_entry (list_front (&right_sema->waiters), struct thread, elem);

  return mlfq_cmp_thread_priority (left_thread, right_thread) < 0;
}

struct semaphore_elem * mlfq_pop_highest_priority_cond_var_waiter (struct list* waiters) {
  struct list_elem * elem = list_pop_max(waiters, mlfq_cond_waiter_less, NULL);

  return list_entry (elem, struct semaphore_elem, elem);
}

int mlfq_thread_priority (struct thread* t) {
  return t->priority;
}