#ifndef __VM__FRAME_TABLE_H_
#define __VM__FRAME_TABLE_H_

#include "userprog/process_vm.h"

struct frame_node;

void init_frame_table(void);
struct frame_node* allocate_user_page(struct vm_node* process_vm);




#endif