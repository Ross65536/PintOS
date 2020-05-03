#ifndef __USERPROG_PROCESS_IMPL_H
#define __USERPROG_PROCESS_IMPL_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/file.h"
// #include "process.h"

struct process_node* add_process (pid_t parent_tid, pid_t pid, uint32_t* pagedir, const char* name, struct file* exec_file);
int collect_process_exit_code (struct process_node* process);

struct process_node* find_process (pid_t pid);



#endif