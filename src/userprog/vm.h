#ifndef __USERPROG_VM__H_
#define __USERPROG_VM__H_

#include <stddef.h>

#define CALL_ARG_ALIGNMENT 4

#define USERLAND_MEM_ERROR ((int) -1)

size_t pointer_alignment_offset (void* ptr, size_t alignment);
void* increment_ptr(void* ptr, int increment);
uint32_t get_userland_double_word (void* ptr, bool* success);
bool get_userland_buffer (void* src_user_buf, void* dest_buf, size_t size);
bool get_userland_string (void* src_user_buf, char* dest_buf, size_t dest_buf_size, size_t* num_written);
bool set_userland_buffer (void* dest_user_buf, void* src_buf, size_t size);

#endif