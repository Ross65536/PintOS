#include "thread.h"
#include "mlfq-scheduler.h"
#include "interrupt.h"
#include "list.h"
#include "synch.h"

#include <kernel/fixed-point.h>
#include <debug.h>

#define NUMBER_QUEUES (PRI_MAX + 1)

#define CUTOFF_RANGE(val, min, max) \
({ \
  __typeof__ (val) _val = (val); \
  __typeof__ (min) _min = (min); \
  __typeof__ (max) _max = (max); \
  _val < _min ? _min : ( _val > _max ? _max : _val); \
})

static struct list mlfqs[NUMBER_QUEUES];
static struct lock mlfqs_lock;
static int ready_threads;

static struct fixed_point load_avg;

void mlfq_scheduler_init (void) {
  for (size_t i = 0; i < NUMBER_QUEUES; i++) {
    list_init (&mlfqs[i]);
  }

  lock_init (&mlfqs_lock);

  struct thread* main_t = running_thread ();
  main_t->thread_mlfq_block.nice = 0;
  main_t->thread_mlfq_block.recent_cpu = fixed_point_build (0);

  load_avg = fixed_point_build (0);
  ready_threads = 0;
}

static void mlfq_update_thread_pri (struct thread_mlfq_block* t) {
  const struct fixed_point recent_er = fixed_point_div_int (t->recent_cpu, 4);  
  const struct fixed_point nice_er = fixed_point_build (t->nice * 2);  

  struct fixed_point pri = fixed_point_build (PRI_MAX);
  pri = fixed_point_sub_real (pri, recent_er);
  pri = fixed_point_sub_real (pri, nice_er);

  const int pri_int = fixed_point_to_nearest_int (pri);
  t->priority = CUTOFF_RANGE (pri_int, PRI_MIN, PRI_MAX);
}

static void mlfq_update_ready_thread_pri (struct thread* t) {
  ASSERT (t->status == THREAD_READY);

  // thread READY
  lock_acquire (&mlfqs_lock);

  const int prev_pri = mlfq_thread_priority(t);
  mlfq_update_thread_pri (&t->thread_mlfq_block);
  const int new_pri = mlfq_thread_priority(t);
  
  if (new_pri != prev_pri) {
    list_remove (&t->elem); // remove thread from previous queue
    list_push_back (&mlfqs[new_pri], &t->elem); // add thread to new queue
  }
  
  lock_release (&mlfqs_lock);
}

void mlfq_thread_init (struct thread *t) {

  struct thread* parent_t = running_thread ();
  t->thread_mlfq_block.nice = parent_t->thread_mlfq_block.nice;
  t->thread_mlfq_block.recent_cpu = parent_t->thread_mlfq_block.recent_cpu;

  mlfq_update_thread_pri (&t->thread_mlfq_block);
}

int mlfq_thread_priority (struct thread* t) {
  return t->thread_mlfq_block.priority;
}


struct thread * mlfq_next_thread_to_run (void) 
{
  lock_acquire (&mlfqs_lock);

  struct thread * next_thread = NULL;
  for (int i = NUMBER_QUEUES - 1; i >= 0; i--) {
    struct list* queue = &mlfqs[i];
    if (! list_empty (queue)) {
      struct list_elem * next_elem = list_pop_front (queue);
      next_thread = list_entry (next_elem, struct thread, elem);

      ready_threads--;
      ASSERT (ready_threads >= 0);

      break;
    }
  }

  lock_release (&mlfqs_lock);
  return next_thread;
}

void mlfq_insert_ready_thread (struct thread* t) {
  const int pri = t->thread_mlfq_block.priority;

  ASSERT (pri >= PRI_MIN && pri <= PRI_MAX);

  lock_acquire (&mlfqs_lock);
  list_push_back (&mlfqs[pri], &t->elem);
  ready_threads++;
  lock_release (&mlfqs_lock);
} 

void mlfq_thread_set_nice (int nice) {
  nice = CUTOFF_RANGE (nice, -20, 20);

  struct thread * t = thread_current ();

  const int inital_pri = mlfq_thread_priority (t);
  
  t->thread_mlfq_block.nice = nice;

  mlfq_update_thread_pri (&t->thread_mlfq_block);
  const int final_pri = mlfq_thread_priority (t);
  if (final_pri < inital_pri) {
    thread_yield ();
  }
}

int mlfq_thread_get_nice (struct thread* t) {
  return t->thread_mlfq_block.nice;
}

int mlfq_thread_get_load_avg () {
  struct fixed_point real = fixed_point_mult_int (load_avg, 100);
  return fixed_point_to_int (real);
}

static void mlfq_thread_quantum_priority_update_func (struct thread *t, void *aux UNUSED) {
  if (t->status != THREAD_READY) {
    mlfq_update_thread_pri (&t->thread_mlfq_block);
    return;
  }

  mlfq_update_ready_thread_pri (t);
}

void mlfq_thread_quantum_tick () {
  thread_foreach (mlfq_thread_quantum_priority_update_func, NULL);
}


static void mlfq_thread_recent_cpu_update (struct thread *t, void *aux UNUSED) {
  const struct fixed_point load_avg_x_2 = fixed_point_mult_int (load_avg, 2);
  const struct fixed_point load_avg_x_2_p_1 = fixed_point_add_int (load_avg_x_2, 1);

  const struct fixed_point load_avg_bit = fixed_point_div_real (load_avg_x_2, load_avg_x_2_p_1);

  struct fixed_point recent_cpu = fixed_point_mult_real (load_avg_bit, t->thread_mlfq_block.recent_cpu);
  recent_cpu = fixed_point_add_int (recent_cpu, t->thread_mlfq_block.nice);

  t->thread_mlfq_block.recent_cpu = recent_cpu;  
}

static void update_load_avg (void) {
  const int num_ready_threads = ready_threads;

  struct fixed_point ready_real = fixed_point_build (num_ready_threads);
  ready_real = fixed_point_div_int (ready_real, 60);

  struct fixed_point load_coeff = fixed_point_build (59);
  load_coeff = fixed_point_div_int (load_coeff, 60);
  struct fixed_point load_real = fixed_point_mult_real (load_avg, load_coeff);

  load_avg = fixed_point_add_real (load_real, ready_real);
}

void mlfq_thread_second_tick (void) {
  update_load_avg ();

  thread_foreach (mlfq_thread_recent_cpu_update, NULL);
}

void mlfq_thread_tick (struct thread_mlfq_block* t) {
  t->recent_cpu = fixed_point_add_int (t->recent_cpu, 1);
}