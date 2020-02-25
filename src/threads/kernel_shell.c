#include <stdbool.h>
#include <stdio.h>
#include <kernel/ringbuffer.h>

#include "kernel_shell.h"
#include "../devices/input.h"

#define BUFFER_SIZE 256

RINGBUFFER(uint8_t, BUFFER_SIZE);

struct ringbuffer_uint8_t ringbuf;

void 
start_kernel_shell (void) 
{
  RINGBUFFER_INIT (ringbuf, BUFFER_SIZE);
  RINGBUFFER_PUSH (ringbuf, 'Z');
  bool popped;
  uint8_t val = RINGBUFFER_POP (uint8_t, ringbuf, popped);
  printf ("%c %x %d", val, val, popped);

  while (true) 
    {
      uint8_t c = input_getc ();
      putchar (c);
      // printf ("%x", c);

    }
}