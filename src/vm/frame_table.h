#ifndef __VM__FRAME_TABLE_H_
#define __VM__FRAME_TABLE_H_

#include "userprog/process_vm.h"
#include "page_common.h"

struct frame_node;

void init_frame_table(void);
struct frame_node* allocate_user_page(void);
void add_frame_vm_page(struct frame_node* node, struct vm_node* page, struct page_common* common);
void destroy_frame(struct frame_node* node);
void print_frame_table(void);
void* get_frame_phys_addr(struct frame_node* node);
void remove_frame_vm_node(struct frame_node* node, struct list_elem* page);
#endif