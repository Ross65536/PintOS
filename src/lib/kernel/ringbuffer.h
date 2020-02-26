/* Ringbuffer templated data type 
   Making use of a GCC C extension.

   Author: Rostyslav Khoptiy
*/

#ifndef __LIB_KERNEL_RINGBUFFER_H
#define __LIB_KERNEL_RINGBUFFER_H

#include <stdbool.h>

/* Ringbuffer data structure. MUST be initialized before use */
#define RINGBUFFER(T, N) \
  struct ringbuffer_##T \
  { \
    T data[N + 1]; \
    int size; \
    int head; \
    int tail; \
  } 

#define __RINGBUFFER_NEXT(val, size) ({ (val + 1) % size; })


// internal. Push head
#define __RINGBUFFER_PUSH(buf, val) \
  buf.data[buf.head] = val; \
  buf.head = __RINGBUFFER_NEXT(buf.head, buf.size);

// internal. Pop tail
#define __RINGBUFFER_POP(buf) ({ buf.tail = __RINGBUFFER_NEXT(buf.tail, buf.size); })


/* initialize struct (MANDATORY) */ 
#define RINGBUFFER_INIT(buf, N) \
  buf.size = N + 1; \
  buf.head = 0; \
  buf.tail = 0; 
  // buf.data is not necessary to initialize to 0

#define RINGBUFFER_IS_EMPTY(buf) ({ buf.head == buf.tail; })

#define RINGBUFFER_IS_FULL(buf) ({ buf.tail == __RINGBUFFER_NEXT(buf.head, buf.size); })


/* 
  Push value to ringbuffer, if full delete last value. Push to head.
*/
#define RINGBUFFER_PUSH(buf, val) \
  if (RINGBUFFER_IS_FULL(buf)) \
    __RINGBUFFER_POP(buf); \
  __RINGBUFFER_PUSH(buf, val);
  
/**
 * Pop value from ringbuffer.  
 * @param has_popped will be set to true if a value has been popped, false otherwise
 * @return the popped value
 */
#define RINGBUFFER_POP(T, buf, has_popped) \
  ({ \
    T val; \
    if (RINGBUFFER_IS_EMPTY(buf)) \
      has_popped = false; \
    else { \
      val = buf.data[buf.tail]; \
      __RINGBUFFER_POP(buf); \
      has_popped = true; \
    } \
    val; \
  })


#endif