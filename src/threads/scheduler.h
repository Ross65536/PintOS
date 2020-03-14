#ifndef THREAD_SCHEDULER__H
#define THREAD_SCHEDULER__H

#include "thread.h"
#include <stdbool.h>

struct thread * pop_highest_priority_thread (struct list* thread_list);

bool should_curr_thread_yield_priority (struct thread * other);

struct semaphore_elem * pop_highest_priority_cond_var_waiter (struct list* waiters);

struct thread * find_thread (struct list* threads, struct thread* target);

#endif