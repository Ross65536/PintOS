#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore 
  {
    unsigned value;             /* Current value. */
    struct list waiters;        /* List of waiting threads. */
  };

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

bool sema_no_waiters (struct semaphore * sema);

/* Lock. */
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
  };

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
void lock_release_with_locking (struct lock *lock, bool donate);
bool lock_held_by_current_thread (const struct lock *);

static inline bool lock_acquire_if_not_held(struct lock * lock) {
  const bool is_held = lock_held_by_current_thread(lock);
  if (!is_held) {
    lock_acquire (lock);
  }

  return is_held;
}

static inline void lock_release_if_not_held(struct lock * lock, bool is_held) {
  if (!is_held) {
    lock_release (lock);
  }
}

/* Condition variable. */
struct condition 
  {
    struct list waiters;        /* List of waiting threads. */
  };

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal_with_yielding (struct condition *, struct lock *, bool can_yield);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

bool cond_no_waiters (struct condition * cond);
/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
