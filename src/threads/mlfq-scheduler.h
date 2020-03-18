#ifndef THREADS_MLSFQ_SCHEDULER__H
#define THREADS_MLSFQ_SCHEDULER__H

#include "scheduler.h"

struct thread * mlfq_pop_highest_priority_thread (struct list* thread_list);
bool mlfq_should_curr_thread_yield_priority (struct thread * other);
struct semaphore_elem * mlfq_pop_highest_priority_cond_var_waiter (struct list* waiters);
int mlfq_thread_priority (struct thread* t);

#endif