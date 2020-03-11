#include <stdbool.h>
#include <stdio.h>
#include <kernel/console.h>
#include <string.h>

#include "kernel_shell.h"
#include "../devices/input.h"

#define BUFFER_SIZE 10

#define PROMPT "CS318> "

static bool 
handle_command(char* line) {
  if (strcmp(line, "whoami") == 0) 
    printf ("I am Ros\n");
  else if (strcmp(line, "exit") == 0) {
    printf ("Bye bye\n");
    return false;
  }
  else 
    printf("You entered: %s\n", line);

  return true;
}

void 
start_kernel_shell (void) 
{
  char buf[BUFFER_SIZE];
  bool cont = true;

  while (cont) 
    {
      printf (PROMPT);
      size_t num_read = kgetline (buf, BUFFER_SIZE);
      if (num_read < BUFFER_SIZE) 
        cont = handle_command(buf);
      else {
        printf("\nERROR: input overflow\n");
      }
    }
}