#include "thread.h"
#include "mlfq-scheduler.h"
#include "interrupt.h"

static struct list ready_list;

void mlfq_scheduler_init (void) {
  list_init (&ready_list);
}

void mlfq_thread_init (struct thread *t) {
  t->priority = PRI_DEFAULT;
}

int mlfq_thread_priority (struct thread* t) {
  return t->priority;
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