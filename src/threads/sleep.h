#ifndef THREADS_SLEEP__H
#define THREADS_SLEEP__H

void
sleep_init (void);

void 
sleep_curr_thread (int64_t target_tick);

void 
threads_unsleep (void * _ UNUSED);


#endif