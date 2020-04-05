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

static bool is_user_ptr_access_valid (void* ptr, size_t size) {
  const uint32_t address = (uint32_t) ptr;
  const uint32_t end_address = address + size;
  void* end_ptr = (void*) end_address;
  uint32_t* pagedir = thread_current()->pagedir;
  
  return size < PGSIZE && is_user_vaddr(end_ptr) && is_user_vaddr (ptr) && 
      is_ptr_page_mapped(pagedir, ptr) && is_ptr_page_mapped (pagedir, end_ptr);
}

/**
 * Return malloc'd buffer with read data from user land on success. Null on failure.
 */
void* get_userland_buffer (void* user_buf_ptr, size_t size) {
  if (! is_user_ptr_access_valid(user_buf_ptr, size)) {
    return NULL;
  }

  void* kbuf = malloc (size);
  if (kbuf == NULL) {
    return NULL;
  }

  memcpy (kbuf, user_buf_ptr, size);
  return kbuf;
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





