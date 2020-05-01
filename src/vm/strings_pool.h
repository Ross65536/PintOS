#ifndef __VM_STRINGS_POOL_H
#define __VM_STRINGS_POOL_H


void strings_pool_init (void);
const char* add_string_pool(const char* string);
void remove_string_pool(const char* string);
void print_strings_pool(void);











#endif