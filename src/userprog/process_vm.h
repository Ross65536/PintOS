#ifndef __USERPROG_PROCESS_BM_H_
#define __USERPROG_PROCESS_BM_H_

#include "userprog/process.h"

struct vm_node;

struct list_elem* get_vm_node_list_elem(struct vm_node* node);

struct vm_node* add_file_backed_vm(struct process_node* process, uint8_t* vaddr, const char* file_path, off_t offset, size_t num_zero_padding, bool readonly, bool exec_file_source);
void print_process_vm(struct process_node* process);
void* activate_vm_page(struct vm_node* node);
void deactivate_vm_node_list(struct list* list);
struct vm_node* add_stack_freestanding_vm(struct process_node* process, uint8_t* vaddr);
struct vm_node* find_vm_node(struct process_node* process, void* address);
void add_process_user_stack_ptr(struct process_node* process, void* address);
void* collect_process_user_stack_ptr(struct process_node* process);
int add_file_mapping(struct process_node* process, int fd, void* addr);
bool unmap_file_mapping(struct process_node* process, int mmapid);


#endif 