#ifndef __MINUNIT_H
#define __MINUNIT_H

#include <stdio.h>
#include <stdlib.h>

/**
 * Based on MinUnit (http://www.jera.com/techinfo/jtns/jtn002.html)
 */

extern int tests_run;

typedef char* (*test_function)(void);

#define mu_assert(message, test) \
  do \
  { \
    if (!(test)) { \
      char* msg = malloc(256); \
      sprintf(msg, __FILE__ ":%d - %s", __LINE__, message); \
      return msg; \
    } \
  } while (0)


#define mu_run_test(test)   \
  do                        \
  {                         \
    char *message = test(); \
    tests_run++;            \
    if (message) \
      return message; \
  } while (0)


#define run_main(test_function) \
  printf("-- Tests: " __FILE__ " --\n"); \
  char *result = test_function(); \
  if (result != 0) \
  { \
    fprintf(stderr, "TESTS FAILED: "); \
    fprintf(stderr, "%s\n", result); \
    free(result); \
    return 1; \
  } \
  else \
    printf("ALL TESTS PASSED\n"); \
  printf("-- Tests run: %d --\n\n", tests_run); \
  return result != 0; \

#endif