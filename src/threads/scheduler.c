#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "list.h"
#include "synch.h"

#define MAX_RECURSION 10


/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

void rr_scheduler_init (void) {
  list_init (&ready_list);
}

static bool thread_list_eq_func (const struct list_elem *list_elem, const struct list_elem *target, void *aux UNUSED) {
  return list_elem == target;
}

static struct thread * find_thread (struct list* threads, struct thread* target) {

  struct list_elem * found = list_find (threads, thread_list_eq_func, &target->elem, NULL); 
  if (found == NULL)
    return NULL;

  return list_entry (found, struct thread, elem);
}

static int rr_thread_priority_recursive (struct thread * t, unsigned int max_recursions) {
  if (t == NULL || max_recursions == 0) {
    return PRI_MIN;
  }

  lock_acquire (&t->rr_thread_block.priority_donors_lock);
  int max_pri = t->rr_thread_block.priority;
  for (size_t i = 0; i < t->rr_thread_block.priority_donors.curr_size; i++) {
    const int parent_pri = rr_thread_priority_recursive (t->rr_thread_block.priority_donors.data[i], max_recursions - 1);
    max_pri = MAX(max_pri, parent_pri); 
  }
  lock_release (&t->rr_thread_block.priority_donors_lock);

  return max_pri;
}

int rr_thread_priority (struct thread * t) {
  ASSERT (t != NULL);

  return rr_thread_priority_recursive (t, MAX_RECURSION);
} 

void rr_try_donate_priority (struct thread* donator, struct thread* recepient) {
  
  lock_acquire (&recepient->rr_thread_block.priority_donors_lock);
  ARRAY_TRY_PUSH (recepient->rr_thread_block.priority_donors, donator);
  lock_release (&recepient->rr_thread_block.priority_donors_lock);

}

void rr_try_undonate_priority (struct list* search_threads, struct thread* target) {
  lock_acquire (&target->rr_thread_block.priority_donors_lock);

  for (size_t i = 0; i < target->rr_thread_block.priority_donors.curr_size; ) {
    struct thread * found = find_thread (search_threads, target->rr_thread_block.priority_donors.data[i]);
    if (found != NULL) {
      
      ARRAY_REMOVE(target->rr_thread_block.priority_donors, i);
    } else {
      i++;
    } 
  }

  lock_release (&target->rr_thread_block.priority_donors_lock);
} 

/* Sets the current thread's priority to NEW_PRIORITY. */
void
rr_thread_set_priority (int new_priority) 
{
  const int curr_pri = thread_get_priority();
  
  // TODO maybe add lock here? Is it necessary?
  thread_current ()->rr_thread_block.priority = new_priority;
  
  const int new_pri = thread_get_priority();
  if (new_pri < curr_pri)
    thread_yield ();
}

void 
rr_thread_init (struct thread *t, int priority) {
  t->rr_thread_block.priority = priority;
  ARRAY_INIT(t->rr_thread_block.priority_donors, DONORS_ARR_SIZE);
  lock_init (&t->rr_thread_block.priority_donors_lock);
}

void rr_insert_ready_thread (struct thread* t) {
  list_push_back (&ready_list, &t->elem);
}

struct thread * rr_next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return NULL;
  
  return pop_highest_priority_thread (&ready_list); 
}