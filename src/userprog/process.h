#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/file.h"

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

extern struct lock filesys_monitor;

void process_impl_init (void);
struct process_node* add_process (pid_t parent_tid, pid_t pid, uint32_t* pagedir, const char* name, struct file* exec_file);
void process_add_exit_code (struct process_node* process, int exit_code);
int collect_process_exit_code (struct process_node* process);
void exit_curr_process(int exit_code, bool should_print_exit_code);
int add_process_open_file (struct process_node* process, struct file* file);
bool process_close_file (struct process_node* process, int fd);
struct file* get_process_open_file (struct process_node* process, int fd);

// process_impl.c
struct process_node;

struct process_node* find_process (pid_t pid);
struct process_node* find_current_thread_process (void);

#endif /* userprog/process.h */
