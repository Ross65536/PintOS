#ifndef THREADS_SLEEP__H
#define THREADS_SLEEP__H

void
sleep_init (void);

void 
sleep_curr_thread (int64_t target_tick);

void 
thread_sleep_tick (int64_t curr_ticks);


#endif