#ifndef __VM_FILE_PAGE_H
#define __VM_FILE_PAGE_H

#include <stddef.h>

#include "filesys/off_t.h"

struct file_page_node {
  const char* file_path;
  off_t offset;
  size_t num_zero_padding;
  struct file * file;
};

struct file_page_node* create_file_page_node(const char* file_path, off_t offset, size_t num_zero_padding);
void destroy_file_page_node(struct file_page_node* node);
void print_file_page_node(struct file_page_node* node);
struct frame_node* load_file_page_frame(struct file_page_node* node);
bool writeback_file_page_frame(struct file_page_node* node, void* page_addr);


unsigned int hash_file_page_node (struct file_page_node* node);
int file_page_node_cmp (struct file_page_node* node_l, struct file_page_node* node_r);





#endif