#ifndef THREADS_MLSFQ_SCHEDULER__H
#define THREADS_MLSFQ_SCHEDULER__H

#include "scheduler.h"
#include "thread.h"
#include <kernel/fixed-point.h>

struct thread_mlfq_block {
  int nice;
  int priority;
  struct fixed_point recent_cpu;  
};


void mlfq_scheduler_init (void);
int mlfq_thread_priority (struct thread* t);
struct thread * mlfq_next_thread_to_run (void);
void mlfq_insert_ready_thread (struct thread* t);
void mlfq_thread_init (struct thread *t);
void mlfq_thread_set_nice (int nice);

int mlfq_thread_get_nice (struct thread* t);

int mlfq_get_load_avg (void);

void mlfq_thread_quantum_tick (void);
void mlfq_thread_second_tick (void);

void mlfq_thread_tick (struct thread_mlfq_block* t);

int mlfq_get_recent_cpu (struct thread_mlfq_block* t);

#endif