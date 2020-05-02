#ifndef __VM_ADCTIVE_FILES_H
#define __VM_ADCTIVE_FILES_H

#include "filesys/off_t.h"
#include "file_page.h"

struct file_offset_mapping;

void init_active_files(void);
struct file_offset_mapping* add_active_file(struct file_page_node* file_page); // file_page will pass ownership here if returned value is != NULL
void print_active_files (void);
void print_file_offset_mapping (struct file_offset_mapping *node);

#endif 