#ifndef __USERPROG_VM__H_
#define __USERPROG_VM__H_

#include <stddef.h>

#define CALL_ARG_ALIGNMENT 4

size_t pointer_alignment_offset (void* ptr, size_t alignment);
void* increment_ptr(void* ptr, int increment);
uint32_t get_userpage_int (void* ptr, bool* success);


#endif