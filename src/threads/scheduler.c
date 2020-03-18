#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "list.h"
#include "synch.h"

#define MAX_RECURSION 10



static bool thread_priority_less (const struct list_elem *left, const struct list_elem *right, void *_ UNUSED) {
  struct thread* left_t = list_entry (left, struct thread, elem);
  struct thread* right_t = list_entry (right, struct thread, elem);

  return rr_cmp_thread_priority (left_t, right_t) < 0;
}

struct thread * rr_pop_highest_priority_thread (struct list* thread_list) {
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
bool rr_should_curr_thread_yield_priority (struct thread * other) { 
  return !intr_context () && rr_cmp_thread_priority (other, thread_current ()) > 0;
}

static bool cond_waiter_less (const struct list_elem *left, const struct list_elem *right, void *_ UNUSED) {
  struct semaphore * left_sema = & (list_entry (left, struct semaphore_elem, elem)->semaphore);
  struct semaphore * right_sema = & (list_entry (right, struct semaphore_elem, elem)->semaphore);

  ASSERT (!sema_no_waiters (left_sema));
  ASSERT (!sema_no_waiters (right_sema)); 

  struct thread * left_thread = list_entry (list_front (&left_sema->waiters), struct thread, elem);
  struct thread * right_thread = list_entry (list_front (&right_sema->waiters), struct thread, elem);

  return rr_cmp_thread_priority (left_thread, right_thread) < 0;
}

struct semaphore_elem * rr_pop_highest_priority_cond_var_waiter (struct list* waiters) {
  struct list_elem * elem = list_pop_max(waiters, cond_waiter_less, NULL);

  return list_entry (elem, struct semaphore_elem, elem);
}


static int thread_priority_recursive (struct thread * t, unsigned int max_recursions) {
  if (t == NULL || max_recursions == 0) {
    return PRI_MIN;
  }

  lock_acquire (&t->priority_donors_lock);
  int max_pri = t->priority;
  for (size_t i = 0; i < t->priority_donors.curr_size; i++) {
    const int parent_pri = thread_priority_recursive (t->priority_donors.data[i], max_recursions - 1);
    max_pri = MAX(max_pri, parent_pri); 
  }
  lock_release (&t->priority_donors_lock);

  return max_pri;
}


int thread_priority (struct thread * t) {
  ASSERT (t != NULL);

  return thread_priority_recursive (t, MAX_RECURSION);
} 

int rr_cmp_thread_priority (struct thread* left_t, struct thread* right_t) {
  return thread_priority (left_t) - thread_priority (right_t);
}

void rr_try_donate_priority (struct thread* donator, struct thread* recepient) {
  
  lock_acquire (&recepient->priority_donors_lock);
  ARRAY_TRY_PUSH (recepient->priority_donors, donator);
  lock_release (&recepient->priority_donors_lock);

}

void rr_try_undonate_priority (struct list* search_threads, struct thread* target) {
  lock_acquire (&target->priority_donors_lock);

  for (size_t i = 0; i < target->priority_donors.curr_size; ) {
    struct thread * found = find_thread (search_threads, target->priority_donors.data[i]);
    if (found != NULL) {
      
      ARRAY_REMOVE(target->priority_donors, i);
    } else {
      i++;
    } 
  }

  lock_release (&target->priority_donors_lock);
} 

/* Sets the current thread's priority to NEW_PRIORITY. */
void
rr_thread_set_priority (int new_priority) 
{
  const int curr_pri = thread_get_priority();
  
  // TODO maybe add lock here? Is it necessary?
  thread_current ()->priority = new_priority;
  
  const int new_pri = thread_get_priority();
  if (new_pri < curr_pri)
    thread_yield ();
}

void 
rr_thread_init (struct thread *t, int priority) {
  t->priority = priority;
  ARRAY_INIT(t->priority_donors, DONORS_ARR_SIZE);
  lock_init (&t->priority_donors_lock);
}