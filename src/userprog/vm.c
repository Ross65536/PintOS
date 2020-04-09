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


/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int get_user_byte (const uint8_t *uaddr)
{
  if (! is_user_vaddr (uaddr)) {
    return USERLAND_MEM_ERROR;
  }

  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool put_user_byte (uint8_t *udst, uint8_t byte)
{
  if (! is_user_vaddr (udst)) {
    return false;
  }

  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != USERLAND_MEM_ERROR;
}


/**
 * Return true if user pointer valid. num_written is the num of characters read from userland excluding \0. 
 * If num_written == dest_buf_size then the buffer has overflowed (but a \0 terminator is still written to it)
 */
bool get_userland_string (void* src_user_buf, char* dest_buf, size_t dest_buf_size, size_t* num_written) {
  uint8_t* user_src = (uint8_t*) src_user_buf;

  for (size_t i = 0; i < dest_buf_size; i++, user_src++) {
    const int access = get_user_byte (user_src);
    if (access == USERLAND_MEM_ERROR) {
      dest_buf[i] = 0;
      *num_written = i;
      return false;
    }

    const char c = access;
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

/**
 * Returns true if valid buffer pointer.
 */
bool get_userland_buffer (void* src_user_buf, void* dest_buf, size_t size) {
  uint8_t* dest = (uint8_t*) dest_buf;
  uint8_t* src = (uint8_t*) src_user_buf;

  for (size_t i = 0; i < size; i++) {
    const int access = get_user_byte (src + i);
    if (access == USERLAND_MEM_ERROR) {
      return false;
    }

    const uint8_t byte = access;
    dest[i] = byte;
  }

  return true;
}

/**
 * Returns true if valid buffer pointer.
 */
bool set_userland_buffer (void* dest_user_buf, void* src_buf, size_t size) {
  uint8_t* dest = (uint8_t*) dest_user_buf;
  uint8_t* src = (uint8_t*) src_buf;

  for (size_t i = 0; i < size; i++) {
    if (! put_user_byte(dest+i, src[i]))
      return false;
  }

  return true;
}

#define DOUBLE_WORD_SIZE (sizeof(uint32_t))

uint32_t get_userland_double_word (void* uptr, bool* success) {
  uint8_t dw[DOUBLE_WORD_SIZE];

  if (! get_userland_buffer(uptr, dw, DOUBLE_WORD_SIZE)) {
    *success = false;
    return -1;
  }

  *success = true;
  const uint32_t value = *((uint32_t*) dw);
  return value;
} 





