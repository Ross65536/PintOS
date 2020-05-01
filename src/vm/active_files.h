#ifndef __VM_ADCTIVE_FILES_H
#define __VM_ADCTIVE_FILES_H

#include "filesys/off_t.h"

struct file_offset_mapping;
struct active_files_list;

void init_active_files(void);
struct file_offset_mapping* add_active_file_offset(struct active_files_list* active_files, const char* file_path, off_t offset);



extern struct active_files_list executable_files;


#endif 