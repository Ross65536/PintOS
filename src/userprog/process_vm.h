#ifndef __USERPROG_PROCESS_BM_H_
#define __USERPROG_PROCESS_BM_H_

#include <kernel/list.h>
#include <kernel/hash.h>

#include "vm/page_common.h"
#include "userprog/process.h"
#include "userprog/process_impl.h"

// fields should not be set directly
struct vm_node {
  struct process_node* process; // parent process's lock
  struct hash_elem hash_elem; // for process vm table
  struct list_elem frame_list_elem; // for frame_table
  struct list_elem mmap_list_elem; // for file mmaps

  struct frame_node* frame;
  uintptr_t page_vaddr;
  struct page_common page_common;
};

bool is_vm_node_dirty(struct vm_node* node);
struct vm_node* add_file_backed_vm(struct process_node* process, uint8_t* vaddr, const char* file_path, off_t offset, size_t num_zero_padding, bool readonly, bool exec_file_source);
void print_process_vm(struct process_node* process);
void* activate_vm_page(struct vm_node* node);
void unmap_vm_node_frame(struct vm_node* node);
struct vm_node* add_stack_freestanding_vm(struct process_node* process, uint8_t* vaddr);
struct vm_node* find_vm_node(struct process_node* process, void* address);
void add_process_user_stack_ptr(struct process_node* process, void* address);
void* collect_process_user_stack_ptr(struct process_node* process);
int add_file_mapping(struct process_node* process, int fd, void* addr);
bool unmap_file_mapping(struct process_node* process, int mmapid);
void print_process_mmaps(struct process_node* process);

#endif 