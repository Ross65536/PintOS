#ifndef __LIB_KERNEL_CONSOLE_H
#define __LIB_KERNEL_CONSOLE_H

#include <stddef.h>

void console_init (void);
void console_panic (void);
void console_print_stats (void);
size_t kgetline (char* bufptr, size_t n);

#endif /* lib/kernel/console.h */
