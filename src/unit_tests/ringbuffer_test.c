/* file minunit_example.c */

#include <stdio.h>
#include "minunit.h"

#include "../lib/kernel/ringbuffer.h"

#define BUFFER_SIZE 3

RINGBUFFER(int, BUFFER_SIZE);

int tests_run = 0;

static char *
test_ringbuffer()
{
  struct ringbuffer_int buf;

  RINGBUFFER_INIT(buf, BUFFER_SIZE);
  MU_ASSERT("init to empty", RINGBUFFER_IS_EMPTY(buf));

  bool popped_1;
  RINGBUFFER_POP(int, buf, popped_1);
  MU_ASSERT("", !popped_1);
  MU_ASSERT("", RINGBUFFER_IS_EMPTY(buf));

  RINGBUFFER_PUSH(buf, 1);
  MU_ASSERT("", !RINGBUFFER_IS_EMPTY(buf));
  MU_ASSERT("", !RINGBUFFER_IS_FULL(buf));

  bool popped_2;
  int val_2 = RINGBUFFER_POP(int, buf, popped_2);
  MU_ASSERT("", RINGBUFFER_IS_EMPTY(buf));
  MU_ASSERT("", val_2 == 1);
  MU_ASSERT("", popped_2);

  RINGBUFFER_PUSH(buf, 1);
  RINGBUFFER_PUSH(buf, 2);
  RINGBUFFER_PUSH(buf, 3);
  MU_ASSERT("", RINGBUFFER_IS_FULL(buf));

  RINGBUFFER_PUSH(buf, 4);
  MU_ASSERT("", RINGBUFFER_IS_FULL(buf));

  bool popped_3;
  int val_3 = RINGBUFFER_POP(int, buf, popped_3);
  MU_ASSERT("", !RINGBUFFER_IS_FULL(buf));
  MU_ASSERT("", val_3 == 2);
  MU_ASSERT("", popped_3);

  return 0;
}

static char *
ringbuffer_tests()
{
  MU_RUN_TEST(test_ringbuffer);
  return 0;
}

int 
main()
{
  MU_RUN_TESTS(ringbuffer_tests);
}
