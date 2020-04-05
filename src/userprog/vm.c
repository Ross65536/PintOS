#include <stdint.h>
#include <stddef.h>
#include <debug.h>
#include <stdbool.h>
#include <string.h>

#include "vm.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"

size_t pointer_alignment_offset (void* ptr, size_t alignment) {
  ASSERT (ptr != NULL);
  ASSERT (alignment % 2 == 0);

  uintptr_t address = (uintptr_t) ptr;

  const size_t offset = address % alignment;
  return offset;
}
 
void* increment_ptr(void* ptr, int increment) {
  uintptr_t addr = (uintptr_t) ptr;

  addr += increment;
  return (void*) addr; 
}

static inline bool is_user_ptr_valid (uint32_t* pagedir, void* ptr) {  
  return is_user_vaddr (ptr) && is_ptr_page_mapped(pagedir, ptr);
}

static bool is_user_ptr_access_valid (void* ptr, size_t size) {
  const uint32_t address = (uint32_t) ptr;
  const uint32_t end_address = address + size;
  void* end_ptr = (void*) end_address;
  uint32_t* pagedir = thread_current()->pagedir;
  
  return size < PGSIZE && is_user_ptr_valid (pagedir, ptr) && is_user_ptr_valid(pagedir, end_ptr);
}

/**
 * Return true if user pointer valid. num_written is the num of characters read from userland excluding \0. 
 * If num_written == dest_buf_size then the buffer has overflowed (but a \0 terminator is still written to it)
 */
bool get_userland_string (void* src_user_buf, char* dest_buf, size_t dest_buf_size, size_t* num_written) {
  char* user_src = (char*) src_user_buf;
  uint32_t* pagedir = thread_current()->pagedir;

  for (size_t i = 0; i < dest_buf_size; i++, user_src++) {
    if (! is_user_ptr_valid (pagedir, user_src)) {
      dest_buf[i] = 0;
      *num_written = i;
      return false;
    }

    const char c = *user_src;
    dest_buf[i] = c;
    if (c == 0) {
      *num_written = i;
      return true;
    } 
  }

  dest_buf[dest_buf_size - 1] = 0;
  *num_written = dest_buf_size;
  return true;
}

bool get_userland_buffer (void* src_user_buf, void* dest_buf, size_t size) {
  if (! is_user_ptr_access_valid(src_user_buf, size)) {
    return false;
  }

  memcpy (dest_buf, src_user_buf, size);
  return true;
}

uint32_t get_userland_double_word (void* uptr, bool* success) {
  if (! is_user_ptr_access_valid(uptr, sizeof(uint32_t))) {
    *success = false;
    return -1;
  }

  *success = true;
  uint32_t* int_ptr = (uint32_t*) uptr;
  return *int_ptr;
} 





