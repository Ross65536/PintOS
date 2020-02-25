#include <stdbool.h>
#include <stdio.h>
#include <kernel/ringbuffer.h>

#include "kernel_shell.h"
#include "../devices/input.h"

#define BUFFER_SIZE 256

RINGBUFFER(uint8_t, BUFFER_SIZE);


void 
start_kernel_shell (void) 
{
  struct ringbuffer_uint8_t ringbuf;
  RINGBUFFER_INIT (ringbuf, BUFFER_SIZE);
  RINGBUFFER_PUSH (ringbuf, 'Z');
  uint8_t val = RINGBUFFER_POP (uint8_t, ringbuf);
  printf ("%c %x", val, val);

  while (true) 
    {
      uint8_t c = input_getc ();
      putchar (c);
      printf ("%x", c);

    }
}