#ifndef __VM_PAGE_COMMON_H
#define __VM_PAGE_COMMON_H

#include <stdbool.h>

#include "vm/file_page.h"
#include "vm/active_files.h"
#include "devices/block.h"

enum page_source_type {
  SHARED_EXECUTABLE,
  FILE_BACKED_EXECUTABLE,
  FILE_BACKED,
  FREESTANDING
};

struct freestanding_page {
  bool swapped;
  block_sector_t swap_number;
};

union page_body {
  struct file_page_node* file_backed;
  struct file_offset_mapping* shared_executable;
  struct freestanding_page freestanding;
};

struct page_common {
  enum page_source_type type;
  union page_body body;
};

bool page_common_eq(struct page_common* l, struct page_common* r);
void print_page_common(struct page_common* page_common);
bool page_common_eq(struct page_common* l, struct page_common* r);

#endif