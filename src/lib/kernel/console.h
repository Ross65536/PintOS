#ifndef __LIB_KERNEL_CONSOLE_H
#define __LIB_KERNEL_CONSOLE_H

#include <stddef.h>

void console_init (void);
void console_panic (void);
void console_print_stats (void);

/** Getline from user. 
 * If the return value is the same as param n the buffer overflowed, meaning no null terminating character was inserted.
 * 
 * @return number of characters read.
 * 
 */
size_t kgetline (char* bufptr, size_t n);

#endif /* lib/kernel/console.h */
