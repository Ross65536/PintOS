#include <kernel/list.h>
#include <debug.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "frame_table.h"
#include "threads/synch.h"
#include "userprog/process_vm.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "page_common.h"
#include "threads/vaddr.h"

struct frame_node {
  struct list_elem list_elem;
  struct list vm_nodes;
  struct lock lock;
  struct page_common page_common;
  void* phys_addr;
  bool dirty_acum;
};

static struct frame_table {
  struct list frames;
  struct lock monitor;
} frame_table;

void init_frame_table(void) {
  list_init (&frame_table.frames);
  lock_init (&frame_table.monitor);
}

static struct frame_node* create_frame_node(void) {
  struct frame_node* node = malloc(sizeof (struct frame_node));
  if (node == NULL) {
    return NULL;
  }

  list_init(&node->vm_nodes);
  node->phys_addr = NULL;
  lock_init(&node->lock);
  node->page_common.type = -1;
  node->dirty_acum = false;

  return node;
}

struct frame_node* allocate_user_page() {
  lock_acquire (&frame_table.monitor);

  struct frame_node* node = create_frame_node();
  if (node == NULL) {
    return NULL;
  }

  void* kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  ASSERT (kpage != NULL); // TODO implement swapping
  node->phys_addr = kpage;

  list_push_back(&frame_table.frames, &node->list_elem);

  lock_release (&frame_table.monitor);

  return node;
}

void add_frame_vm_page(struct frame_node* node, struct vm_node* page, struct page_common* common) {
  ASSERT (node != NULL);
  ASSERT (page != NULL);
  ASSERT (common != NULL);

  lock_acquire(&node->lock);
 
  if (list_empty(&node->vm_nodes)) {
    node->page_common = *common;
  } else {
    if (is_page_common_shared_file(common)) {
      ASSERT (page_common_eq(&node->page_common, common));
    } else {
      NOT_REACHED();
    }
  }

  list_push_back(&node->vm_nodes, &page->frame_list_elem);

  lock_release(&node->lock); 
}

static void collect_dirty_bits(struct frame_node* node) {
  ASSERT (node != NULL);
  ASSERT (lock_held_by_current_thread (&node->lock));

  for (struct list_elem *e = list_begin (&node->vm_nodes); e != list_end (&node->vm_nodes); e = list_next (e)) {
    struct vm_node* vm_node = list_entry(e, struct vm_node, frame_list_elem);
    if (is_vm_node_dirty (vm_node))
      node->dirty_acum = true;
  }
}

void remove_frame_vm_node(struct frame_node* node, struct list_elem* page) {
  lock_acquire(&node->lock);
  collect_dirty_bits(node);
  list_remove(page);
  lock_release(&node->lock);
}

void destroy_frame(struct frame_node* node) {

  ASSERT (node != NULL);
  
  lock_acquire(&node->lock);

  lock_acquire (&frame_table.monitor);
  list_remove (&node->list_elem);
  lock_release (&frame_table.monitor);

  collect_dirty_bits(node);

  for (struct list_elem *e = list_begin (&node->vm_nodes); e != list_end (&node->vm_nodes); e = list_remove (e)) {
    struct vm_node* node = list_entry(e, struct vm_node, frame_list_elem);
    unmap_vm_node_frame(node);
  }

  if (is_page_common_shared_file(&node->page_common)) {
    unload_file_offset_mapping_frame(get_page_common_shared_active_file(&node->page_common));
  }

  if (node->page_common.type == SHARED_WRITABLE_FILE && node->dirty_acum) {
    writeback_file_page_frame(get_file_offset_mapping_file_page(node->page_common.body.shared_writable_file), node->phys_addr);
  }

  // TODO implement swapping

  palloc_free_page(node->phys_addr);
  lock_release(&node->lock);

  free(node);
}



void print_frame_table(void) {
  lock_acquire (&frame_table.monitor);

  printf ("Frame-table (len=%lu): \n", list_size (&frame_table.frames));

  for (struct list_elem *e = list_begin (&frame_table.frames); e != list_end (&frame_table.frames); e = list_next (e)) {
    struct frame_node* node = list_entry(e, struct frame_node, list_elem);

    printf ("(frame=%x, vm_refs=%lu, body=", (uintptr_t) node->phys_addr, list_size(&node->vm_nodes));
    print_page_common (&node->page_common);
    printf(")\n");
  }

  printf("---\n");

  lock_release (&frame_table.monitor);
}

void* get_frame_phys_addr(struct frame_node* node) {
  ASSERT (node != NULL);
  return node->phys_addr;
}
