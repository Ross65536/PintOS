#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);


// process-impl
typedef tid_t pid_t;
#define PID_ERROR ((pid_t) -1)

#define PROCESS_MAX_NAME 32
#define BAD_EXIT_CODE -1
#define MAX_PROCESS_ARGS_SIZE 256

void process_impl_init (void);
void add_process (tid_t parent_tid, tid_t tid, const char* name);
void process_add_exit_code (tid_t tid, int exit_code);
int collect_process_exit_code (tid_t tid);
void exit_curr_process(int exit_code, bool should_print_exit_code);


#endif /* userprog/process.h */
