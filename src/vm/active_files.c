
#include <kernel/hash.h>
#include <debug.h>
#include <string.h>
#include <stdio.h>

#include "threads/malloc.h"
#include "threads/synch.h"
#include "active_files.h"
#include "filesys/off_t.h"
#include "file_page.h"

#define MAX_FILEPATH 256

struct file_offset_mapping {
  struct hash_elem elem;
  struct file_page_node* file_page;
  unsigned int process_ref_count;
};

static struct file_offset_mapping* create_file_offset_mapping(struct file_page_node* file_page) {
  ASSERT (file_page != NULL);

  struct file_offset_mapping* node = malloc(sizeof(struct file_offset_mapping));
  if (node == NULL) {
    return NULL;
  }

  node->file_page = file_page;
  node->process_ref_count = 0;
  
  return node;
}

// active_files_list

static unsigned int active_files_list_hash (const struct hash_elem *e, void *_ UNUSED) {
  struct file_offset_mapping *node = hash_entry (e, struct file_offset_mapping, elem);
  return hash_file_page_node (node->file_page);
}

static bool active_files_list_less (const struct hash_elem *l, const struct hash_elem *r, void *_ UNUSED) {
  struct file_offset_mapping *node_l = hash_entry (l, struct file_offset_mapping, elem);
  struct file_offset_mapping *node_r = hash_entry (r, struct file_offset_mapping, elem);

  return file_page_node_cmp (node_l->file_page, node_r->file_page) < 0;
}
 
struct active_files_list {
  struct hash active_files;
  struct lock monitor;
} active_list;

static bool init_active_files_list(struct active_files_list* active_files) {
  lock_init (&active_files->monitor);
  return hash_init (&active_files->active_files, active_files_list_hash, active_files_list_less, NULL);
}

static struct file_offset_mapping* find_file_offset_mapping(struct hash* hash, struct file_page_node* file_page) {
  struct file_offset_mapping find;
  find.file_page = file_page;


  struct hash_elem* found = hash_find (hash, &find.elem);
  if (found == NULL) {
    return NULL;
  }

  return hash_entry (found, struct file_offset_mapping, elem);
}


////////////
/// impl
////////////

void init_active_files() {
  const bool ok = init_active_files_list(&active_list);
  ASSERT (ok);
}

struct file_offset_mapping* add_active_file(struct file_page_node* file_page) {

  lock_acquire (&active_list.monitor);

  struct file_offset_mapping* node = find_file_offset_mapping(&active_list.active_files, file_page);
  if (node == NULL) {
    node = create_file_offset_mapping(file_page);
    if (node == NULL) {
      return NULL;
    }

    ASSERT (hash_insert (&active_list.active_files, &node->elem) == NULL);
  } else {
    destroy_file_page_node(file_page);
  }

  node->process_ref_count++;

  lock_release (&active_list.monitor);
  return node;
}

void print_file_offset_mapping (struct file_offset_mapping *node) {
  printf ("(");
  print_file_page_node (node->file_page);
  printf (", proc_ref=%d)", node->process_ref_count);
}

static void file_offset_mapping_print (struct hash_elem *e, void *_ UNUSED) {
  struct file_offset_mapping *node = hash_entry (e, struct file_offset_mapping, elem);

  print_file_offset_mapping(node);
  printf ("\n");
}

void print_active_files () {
  lock_acquire (&active_list.monitor);

  printf ("Active files (len=%lu): \n", hash_size(&active_list.active_files));
  hash_apply (&active_list.active_files, file_offset_mapping_print);
  printf("---\n");

  lock_release (&active_list.monitor);
}