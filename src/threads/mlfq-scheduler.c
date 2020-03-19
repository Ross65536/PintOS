#include "thread.h"
#include "mlfq-scheduler.h"
#include "interrupt.h"

static struct list ready_list;

void mlfq_scheduler_init (void) {
  list_init (&ready_list);

  running_thread ()->thread_mlfq_block.nice = 0;
}

void mlfq_thread_init (struct thread *t) {
  // TODO
  // t->priority = PRI_DEFAULT;
  t->thread_mlfq_block.nice = running_thread ()->thread_mlfq_block.nice;
}

int mlfq_thread_priority (struct thread* t) {
  // TODO
  return 0;
}


struct thread * mlfq_next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return NULL;
  
  return pop_highest_priority_thread (&ready_list); 
}

void mlfq_insert_ready_thread (struct thread* t) {
  list_push_back (&ready_list, &t->elem);
} 

void mlfq_thread_set_nice (int nice) {
  struct thread * t = thread_current ();

  const int inital_pri = mlfq_thread_priority (t);
  
  t->thread_mlfq_block.nice = nice;

  // TODO: calc new thread pri

  const int final_pri = mlfq_thread_priority (t);
  if (final_pri < inital_pri) {
    thread_yield ();
  }

}

int mlfq_thread_get_nice (struct thread* t) {
  return t->thread_mlfq_block.nice;
}