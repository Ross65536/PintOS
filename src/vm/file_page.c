#include <kernel/hash.h>
#include <string.h>
#include <stdio.h>

#include "filesys/filesys.h"
#include "filesys/off_t.h"
#include "file_page.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "strings_pool.h"
#include "frame_table.h"



struct file_page_node* create_file_page_node(const char* file_path, off_t offset, size_t num_zero_padding) {
  ASSERT (num_zero_padding <= PGSIZE);

  const bool holding = lock_acquire_if_not_held(&filesys_monitor);
  struct file * file = filesys_open(file_path);
  lock_release_if_not_held (&filesys_monitor, holding);

  if (file == NULL) {
    return NULL;
  }

  struct file_page_node* node = malloc (sizeof (struct file_page_node));
  if (node == NULL) {
    return NULL;
  }

  const char* copy = add_string_pool (file_path);
  node->file = file;
  node->file_path = copy;
  node->offset = offset;
  node->num_zero_padding = num_zero_padding;

  return node;
}

void destroy_file_page_node(struct file_page_node* node) {
  remove_string_pool(node->file_path);

  const bool holding = lock_acquire_if_not_held (&filesys_monitor);
  file_close(node->file);
  lock_release_if_not_held (&filesys_monitor, holding);

  free (node);
}

unsigned int hash_file_page_node (struct file_page_node* node) {
  return hash_string(node->file_path) ^ hash_int(node->offset) ^ hash_int(node->num_zero_padding);
}

int file_page_node_cmp (struct file_page_node* node_l, struct file_page_node* node_r) {
  const int path_cmp = strcmp (node_l->file_path, node_r->file_path);
  if (path_cmp != 0) {
    return path_cmp;
  }

  const int offset_cmp = node_l->offset - node_r->offset;
  if (offset_cmp != 0) {
    return offset_cmp;
  }

  return node_l->num_zero_padding - node_r->num_zero_padding;
}

struct frame_node* load_file_page_frame(struct file_page_node* node) {


  struct frame_node* frame = allocate_user_page();
  void* addr = get_frame_phys_addr(frame);
  const off_t num_read = PGSIZE - node->num_zero_padding;

  lock_acquire (&filesys_monitor);
  file_seek(node->file, node->offset);
  if (file_read (node->file, addr, num_read) != num_read) {
    destroy_frame(frame);
    frame = NULL;
  }
 
  lock_release (&filesys_monitor);

  return frame;
}

bool writeback_file_page_frame(struct file_page_node* node, void* page_addr) {

  const off_t num_write = PGSIZE - node->num_zero_padding;
  lock_acquire (&filesys_monitor);
  file_seek(node->file, node->offset);
  const bool ok = file_write (node->file, page_addr, num_write) == num_write;
  ASSERT (ok);
  lock_release (&filesys_monitor);

  return ok;
}

void print_file_page_node(struct file_page_node* node) { 
  printf ("(path=%s, off=%d, zeros=%lu)", node->file_path, node->offset, node->num_zero_padding);
}