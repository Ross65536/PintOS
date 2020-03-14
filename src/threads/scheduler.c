#include "thread.h"
#include "scheduler.h"

static bool thread_priority_less (const struct list_elem *left, const struct list_elem *right, void *_ UNUSED) {
  const int left_pri = list_entry (left, struct thread, elem)->priority;
  const int right_pri = list_entry (right, struct thread, elem)->priority;

  return left_pri < right_pri;
}



struct thread * pop_highest_priority_thread (struct list* thread_list) {
  ASSERT (! list_empty(thread_list));

  struct list_elem * next_thread = list_max (thread_list, thread_priority_less, NULL);
  list_remove (next_thread);

  return list_entry (next_thread, struct thread, elem);
}