#ifndef PROCESS_EXIT__H
#define PROCESS_EXIT__H

#include "threads/thread.h"

#define PROCESS_MAX_NAME 32

#define BAD_EXIT_CODE -1

void process_exit_init (void);
void add_process_exit_elem (tid_t parent_tid, tid_t tid, const char* name);
void process_add_exit_code (tid_t tid, int exit_code);
int collect_process_exit_code (tid_t tid);
void print_exit_code (tid_t tid);

#endif