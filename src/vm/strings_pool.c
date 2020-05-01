#include <stddef.h>
#include <stdbool.h>
#include <kernel/hash.h>
#include <debug.h>
#include <string.h>
#include <stdio.h>

#include "threads/synch.h"
#include "strings_pool.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

#define MAX_STRING_SIZE PGSIZE

struct string_node {
  struct hash_elem elem;

  const char* string;
  size_t ref_count;
};

static struct strings_pool {
  struct hash strings;
  struct lock monitor;
} pool;

static unsigned int strings_pool_hash (const struct hash_elem *e, void *_ UNUSED) {
  struct string_node *node = hash_entry (e, struct string_node, elem);
  return hash_string (node->string);
}

static bool strings_pool_less (const struct hash_elem *l, const struct hash_elem *r, void *_ UNUSED) {
  struct string_node *node_l = hash_entry (l, struct string_node, elem);
  struct string_node *node_r = hash_entry (r, struct string_node, elem);

  return strcmp (node_l->string, node_r->string) < 0;
}

void strings_pool_init () {
  lock_init (&pool.monitor);
  const bool ok = hash_init (&pool.strings, strings_pool_hash, strings_pool_less, NULL);
  ASSERT(ok);
}

static struct string_node* find_string_node(struct hash* hash, const char* string) {
  struct string_node node;
  node.string = string;

  struct hash_elem* found = hash_find (hash, &node.elem);
  if (found == NULL) {
    return NULL;
  }

  return hash_entry (found, struct string_node, elem);
}

static struct string_node* create_string_node(const char* string) {
  size_t str_size = strlen(string) + 1;
  if (str_size > MAX_STRING_SIZE) {
    return NULL;
  }

  char* str_cpy = malloc(str_size);
  if (str_cpy == NULL) {
    return NULL;
  }

  strlcpy (str_cpy, string, str_size);
  struct string_node* node = malloc(sizeof(struct string_node));
  if (node == NULL) {
    return NULL;
  }

  node->string = str_cpy;
  node->ref_count = 0;
  
  return node;
}

const char* add_string_pool(const char* string) {
  lock_acquire (&pool.monitor);

  struct string_node* node = find_string_node(&pool.strings, string);
  if (node == NULL) {
    node = create_string_node(string);
    if (node == NULL) {
      return NULL;
    }

    hash_insert (&pool.strings, &node->elem);
  }

  node->ref_count++;

  lock_release (&pool.monitor);

  return node->string;
} 

void remove_string_pool(const char* string) {
  lock_acquire (&pool.monitor);

  struct string_node* node = find_string_node(&pool.strings, string);
  if (node == NULL) {
    lock_release (&pool.monitor);
    return;
  }

  node->ref_count--;
  if (node->ref_count == 0) {
    free((void*) node->string);
    free(node);
  }


  lock_release (&pool.monitor);
}

static void strings_pool_print (struct hash_elem *e, void *_ UNUSED) {
  struct string_node *node = hash_entry (e, struct string_node, elem);

  printf("(string=%s, refs=%lu), ", node->string, node->ref_count);
}

void print_strings_pool() {
  lock_acquire (&pool.monitor);

  printf ("Strings pool: ");
  hash_apply (&pool.strings, strings_pool_print);
  printf("\n");

  lock_release (&pool.monitor);
}