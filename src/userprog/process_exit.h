#ifndef PROCESS_EXIT__H
#define PROCESS_EXIT__H

#include "threads/thread.h"

void process_exit_init (void);
void add_process_exit_elem (tid_t parent_tid, tid_t tid);
void process_add_exit_code (tid_t tid, int exit_code);
int collect_process_exit_code (tid_t tid);

#endif