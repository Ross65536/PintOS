#ifndef THREADS_MLSFQ_SCHEDULER__H
#define THREADS_MLSFQ_SCHEDULER__H

#include "scheduler.h"

void mlfq_scheduler_init (void);
int mlfq_thread_priority (struct thread* t);
struct thread * mlfq_next_thread_to_run (void);
void mlfq_insert_ready_thread (struct thread* t);
void mlfq_thread_init (struct thread *t);
#endif