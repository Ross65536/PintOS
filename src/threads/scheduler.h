#ifndef THREAD_SCHEDULER__H
#define THREAD_SCHEDULER__H

#include "thread.h"
#include <stdbool.h>

#define DONORS_ARR_SIZE 5

struct thread;
ARRAY_TYPE(thread_arr, struct thread *, DONORS_ARR_SIZE);

struct rr_thread_block {
  struct lock priority_donors_lock;
  struct array_thread_arr priority_donors;
};

struct thread * rr_pop_highest_priority_thread (struct list* thread_list);

bool rr_should_curr_thread_yield_priority (struct thread * other);

struct semaphore_elem * rr_pop_highest_priority_cond_var_waiter (struct list* waiters);

int rr_thread_priority (struct thread * t);

void rr_try_donate_priority (struct thread* donator, struct thread* recepient);

void rr_try_undonate_priority (struct list* search_threads, struct thread* target);

void rr_thread_set_priority (int new_priority);

void rr_thread_init (struct thread *t, int priority);

#endif