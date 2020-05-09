#ifndef __VM_ADCTIVE_FILES_H
#define __VM_ADCTIVE_FILES_H

#include "filesys/off_t.h"
#include "file_page.h"

struct file_offset_mapping;
struct active_files_list;

void init_active_files(void);
struct file_offset_mapping* add_active_file(struct active_files_list* active_list, struct file_page_node* file_page); // file_page will be taken ownership here if returned value != NULL
void print_active_files (struct active_files_list* active_list);
void print_file_offset_mapping (struct file_offset_mapping *node);
void destroy_active_file (struct active_files_list* active_list, struct file_offset_mapping *node);
struct frame_node* load_file_offset_mapping_page (struct file_offset_mapping *node);
void unload_file_offset_mapping_frame (struct file_offset_mapping *node);

extern struct active_files_list readonly_files;
extern struct active_files_list writable_files;

#endif 