#ifndef __LIB_KERNEL_CONSOLE_H
#define __LIB_KERNEL_CONSOLE_H

#include <stddef.h>
#include <stdint.h>

void console_init (void);
void console_panic (void);
void console_print_stats (void);
size_t kgetline (char* bufptr, size_t n);
void input_getchars (uint8_t* dest_buf, size_t buf_size);

#endif /* lib/kernel/console.h */
