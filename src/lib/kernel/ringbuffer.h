#ifndef __LIB_KERNEL_RINGBUFFER_H
#define __LIB_KERNEL_RINGBUFFER_H

/* Ringbuffer templated data type 
   Making use of a GCC C extension.

   Author: Rostyslav Khoptiy
*/

// if used as global it is automatically initialized
#define RINGBUFFER(T, buf_length) \
  struct ringbuffer_##T \
  { \
    T data[buf_length]; \
    int length; \
    int head; \
    int tail; \
  } 

#define RINGBUFFER_INIT(buf, buf_length) \
  buf.length = buf_length; \
  buf.head = 0; \ 
  buf.tail = 0; 
  // buf.data is not necessary to initialize to 0


#define RINGBUFFER_PUSH(buf, val) \
  buf.data[buf.head] = val; \
  buf.head = (buf.head + 1) % buf.length;

#define RINGBUFFER_POP(T, buf) \
({ \
    T val = buf.data[buf.tail]; \
    buf.tail = (buf.tail + 1) % buf.length; \
    val; \
})
















#endif