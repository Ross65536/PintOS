#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "list.h"
#include "synch.h"





static bool thread_priority_less (const struct list_elem *left, const struct list_elem *right, void *_ UNUSED) {
  struct thread* left_t = list_entry (left, struct thread, elem);
  struct thread* right_t = list_entry (right, struct thread, elem);

  return cmp_thread_priority (left_t, right_t) < 0;
  
  // const int left_pri = thread_priority(left_t);
  // const int right_pri = thread_priority(right_t);

  // return left_pri < right_pri;
}

struct thread * pop_highest_priority_thread (struct list* thread_list) {
  struct list_elem * next_thread = list_pop_max (thread_list, thread_priority_less, NULL);

  return list_entry (next_thread, struct thread, elem);
}

static bool thread_list_eq_func (const struct list_elem *list_elem, const struct list_elem *target, void *aux UNUSED) {
  return list_elem == target;
}

struct thread * find_thread (struct list* threads, struct thread* target) {

  struct list_elem * found = list_find (threads, thread_list_eq_func, &target->elem, NULL); 
  if (found == NULL)
    return NULL;

  return list_entry (found, struct thread, elem);
}

/** Should be followed by call to thread_yield()
 */
bool should_curr_thread_yield_priority (struct thread * other) { 
  return !intr_context () && cmp_thread_priority (other, thread_current ()) > 0;
}

static bool cond_waiter_less (const struct list_elem *left, const struct list_elem *right, void *_ UNUSED) {
  struct semaphore * left_sema = & (list_entry (left, struct semaphore_elem, elem)->semaphore);
  struct semaphore * right_sema = & (list_entry (right, struct semaphore_elem, elem)->semaphore);

  ASSERT (!sema_no_waiters (left_sema));
  ASSERT (!sema_no_waiters (right_sema)); 

  struct thread * left_thread = list_entry (list_front (&left_sema->waiters), struct thread, elem);
  struct thread * right_thread = list_entry (list_front (&right_sema->waiters), struct thread, elem);

  return cmp_thread_priority (left_thread, right_thread) < 0;
}

struct semaphore_elem * pop_highest_priority_cond_var_waiter (struct list* waiters) {
  struct list_elem * elem = list_pop_max(waiters, cond_waiter_less, NULL);

  return list_entry (elem, struct semaphore_elem, elem);
}