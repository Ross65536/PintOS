#ifndef THREAD_SCHEDULER__H
#define THREAD_SCHEDULER__H

#include "thread.h"
#include <stdbool.h>

#define DONORS_ARR_SIZE 5

struct thread;
ARRAY_TYPE(thread_arr, struct thread *, DONORS_ARR_SIZE);

struct rr_thread_block {
  int priority;                       /* Priority. */
  struct lock priority_donors_lock;
  struct array_thread_arr priority_donors;
};

void rr_scheduler_init (void);

int rr_thread_priority (struct thread * t);

void rr_try_donate_priority (struct thread* donator, struct thread* recepient);

void rr_try_undonate_priority (struct list* search_threads, struct thread* target);

void rr_thread_set_priority (int new_priority);

void rr_thread_init (struct thread *t, int priority);

void rr_insert_ready_thread (struct thread* t);

struct thread * rr_next_thread_to_run (void);

#endif