#ifndef __LIB_KERNEL_ARRAY__H
#define __LIB_KERNEL_ARRAY__H

#include <stddef.h>
#include <stdbool.h>

// TNAME is to allow multiple struct definitions
#define ARRAY_TYPE(TNAME, T, N) \
  struct array_##TNAME \
  { \
    T data[N]; \
    size_t total_size; \
    size_t curr_size; \
  }

#define ARRAY_INIT(arr, N) \
  arr.curr_size = 0; \
  arr.total_size = N;

#define ARRAY_IS_FULL(arr) ({ arr.total_size == arr.curr_size; })

#define ARRAY_IS_EMPTY(arr) ({ arr.curr_size == 0; })

#define ARRAY_TRY_PUSH(arr, val) \
  ({ \
    const bool can_push = ! ARRAY_IS_FULL(arr); \
    if ( can_push ) { \
      arr.data[arr.curr_size] = val; \
      arr.curr_size++; \
    } \
    can_push; \
  })

#define ARRAY_REMOVE(arr, index) ({ \
  const bool can_remove = index < arr.curr_size && ! ARRAY_IS_EMPTY (arr); \
  if (can_remove) { \
      for (size_t __idx = index; __idx < arr.curr_size - 1; __idx++) { \
        arr.data[__idx] = arr.data[__idx + 1]; \
      } \
      arr.curr_size--; \
  } \
  can_remove; \
})












#endif