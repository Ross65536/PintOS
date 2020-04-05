#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);


// process-impl
#define PROCESS_MAX_NAME 32
#define BAD_EXIT_CODE -1

void process_impl_init (void);
void add_process (tid_t parent_tid, tid_t tid, const char* name);
void process_add_exit_code (tid_t tid, int exit_code);
int collect_process_exit_code (tid_t tid);
void exit_curr_process(int exit_code, bool should_print_exit_code);

#define PROCESS_ARGS_SIZE 256
#define MAX_ARGS 30
struct start_process_arg {
  char filename[PROCESS_ARGS_SIZE];
  size_t num_args;
  char* args[MAX_ARGS];
  tid_t parent_tid;
};

void parse_executable_command (struct start_process_arg* process_args, const char* command);

#endif /* userprog/process.h */
