#include <kernel/hash.h>
#include <string.h>
#include <stdio.h>

#include "filesys/off_t.h"
#include "file_page.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "strings_pool.h"

struct file_page_node {
  const char* file_path;
  off_t offset;
  size_t num_zero_padding;
};

struct file_page_node* create_file_page_node(const char* file_path, off_t offset, size_t num_zero_padding) {
  ASSERT (num_zero_padding <= PGSIZE);

  struct file_page_node* node = malloc (sizeof (struct file_page_node));
  if (node == NULL) {
    return NULL;
  }

  const char* copy = add_string_pool (file_path);
  node->file_path = copy;
  node->offset = offset;
  node->num_zero_padding = num_zero_padding;

  return node;
}

void destroy_file_page_node(struct file_page_node* node) {
  remove_string_pool(node->file_path);
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

void print_file_page_node(struct file_page_node* node) { 
  printf ("(path=%s, off=%d, zeros=%lu)", node->file_path, node->offset, node->num_zero_padding);
}