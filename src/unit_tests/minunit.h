#include <stdio.h>
#include <stdlib.h>

/**
 * Based on MinUnit (http://www.jera.com/techinfo/jtns/jtn002.html)
 */


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



extern int tests_run;
