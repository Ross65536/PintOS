#include <kernel/list.h>
#include <debug.h>
#include <stddef.h>
#include <stdbool.h>

#include "frame_table.h"
#include "threads/synch.h"
#include "userprog/process_vm.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

struct frame_node {
  struct list_elem list_elem;
  struct list vm_nodes;

  void* phys_addr;

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

  return node;
}

struct frame_node* allocate_user_page(struct vm_node* process_vm) {
  lock_acquire (&frame_table.monitor);

  struct frame_node* node = create_frame_node();
  if (node == NULL) {
    return NULL;
  }

  list_push_back(&node->vm_nodes, get_vm_node_list_elem(process_vm));
  
  void* kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  ASSERT (kpage != NULL); // TODO implement swapping
  node->phys_addr = kpage;


  lock_release (&frame_table.monitor);

  return node;
}

