#ifndef __USERPROG_PROCESS_BM_H_
#define __USERPROG_PROCESS_BM_H_

#include "userprog/process.h"

struct vm_node;

struct list_elem* get_vm_node_list_elem(struct vm_node* node);
struct vm_node* add_file_backed_vm(struct process_node* process, uint8_t* vaddr, const char* file_path, off_t offset, size_t num_zero_padding, bool readonly, bool exec_file_source);
void print_process_vm(struct process_node* process);



#endif