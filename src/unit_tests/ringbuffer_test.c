/* file minunit_example.c */

#include <stdio.h>
#include "minunit.h"

#include "../lib/kernel/ringbuffer.h"

#define BUFFER_SIZE 3

RINGBUFFER(int, BUFFER_SIZE);

int tests_run = 0;

static char *
test_ringbuffer_empty()
{
  struct ringbuffer_int buf;

  RINGBUFFER_INIT(buf, BUFFER_SIZE);
  mu_assert("init to empty", RINGBUFFER_IS_EMPTY(buf));

  bool popped_1;
  RINGBUFFER_POP(int, buf, popped_1);
  mu_assert("", !popped_1);
  mu_assert("", RINGBUFFER_IS_EMPTY(buf));

  RINGBUFFER_PUSH(buf, 1);
  mu_assert("", !RINGBUFFER_IS_EMPTY(buf));
  mu_assert("", !RINGBUFFER_IS_FULL(buf));

  bool popped_2;
  int val_2 = RINGBUFFER_POP(int, buf, popped_2);
  mu_assert("", RINGBUFFER_IS_EMPTY(buf));
  mu_assert("", val_2 == 1);
  mu_assert("", popped_2);

  RINGBUFFER_PUSH(buf, 1);
  RINGBUFFER_PUSH(buf, 2);
  mu_assert("", RINGBUFFER_IS_FULL(buf));

  RINGBUFFER_PUSH(buf, 3);
  mu_assert("", RINGBUFFER_IS_FULL(buf));

  bool popped_3;
  int val_3 = RINGBUFFER_POP(int, buf, popped_3);
  mu_assert("", !RINGBUFFER_IS_FULL(buf));
  mu_assert("", val_3 == 2);
  mu_assert("", popped_3);

  return 0;
}

static char *ringbuffer_tests()
{
  mu_run_test(test_ringbuffer_empty);
  return 0;
}

int main()
{
  run_main(ringbuffer_tests);
}
