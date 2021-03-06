#ifndef __VM_PAGE_COMMON_H
#define __VM_PAGE_COMMON_H

#include <stdbool.h>

#include "vm/file_page.h"
#include "vm/active_files.h"
#include "devices/block.h"

enum page_source_type {
  SHARED_READONLY_FILE, // can be executable chunk or mmap
  SHARED_WRITABLE_FILE, // only writable mmaps
  FILE_BACKED_EXECUTABLE_STATIC, 
  FREESTANDING
};

struct swappable_page {
  bool is_swapped;
  block_sector_t swap_number;
};

struct file_backed {
  struct file_page_node* file;
  struct swappable_page swap;
};

union page_body {
  struct file_offset_mapping* shared_readonly_file;
  struct file_offset_mapping* shared_writable_file;
  struct file_backed file_backed_executable_static;
  struct swappable_page freestanding;
};

struct page_common {
  enum page_source_type type;
  union page_body body;
};

static inline bool is_page_common_shared_file(struct page_common* common) {
  const enum page_source_type type = common->type;
  return type == SHARED_READONLY_FILE || type == SHARED_WRITABLE_FILE;
}

static inline struct file_offset_mapping* get_page_common_shared_active_file(struct page_common* common) {
  ASSERT (is_page_common_shared_file(common));

  return common->type == SHARED_READONLY_FILE ? 
      common->body.shared_readonly_file : common->body.shared_writable_file;
}

static inline struct page_common init_freestanding(void) {
  struct page_common ret = {
    .type = FREESTANDING, 
    .body = {
      .freestanding = {
        .swap_number = -1,
        .is_swapped = false
      }
    }
  };
  return ret;
}

static inline bool is_page_common_readonly(struct page_common* page) {
  const enum page_source_type type = page->type;
  return type == SHARED_READONLY_FILE;
}

static inline struct page_common init_shared_writable_file_backed(struct file_offset_mapping* shared_page) {
  struct page_common ret = {
    .type = SHARED_WRITABLE_FILE, 
    .body = {
      .shared_writable_file = shared_page
    }
  };
  return ret;
}

static inline struct page_common init_shared_readonly_file_backed(struct file_offset_mapping* shared_page) {
  struct page_common ret = {
    .type = SHARED_READONLY_FILE, 
    .body = {
      .shared_readonly_file = shared_page
    }
  };
  return ret;
}

static inline struct page_common init_file_backed_executable_static(struct file_page_node* file_page) {
  struct page_common ret = {
    .type = FILE_BACKED_EXECUTABLE_STATIC, 
    .body = {
      .file_backed_executable_static = {
        .file = file_page,
        .swap = {
          .swap_number = -1,
          .is_swapped = false
        }
      }
    }
  };
  return ret;
}

bool page_common_eq(struct page_common* l, struct page_common* r);
void print_page_common(struct page_common* page_common);
bool page_common_eq(struct page_common* l, struct page_common* r);

#endif