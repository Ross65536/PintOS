#ifndef __VM_PAGE_COMMON_H
#define __VM_PAGE_COMMON_H

#include <stdbool.h>

#include "vm/file_page.h"
#include "vm/active_files.h"
#include "devices/block.h"

enum page_source_type {
  SHARED_EXECUTABLE, // can also be a readonly mmap
  FILE_BACKED_EXECUTABLE, 
  FILE_BACKED, // only writable mmaps
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
  struct file_backed file_backed;
  struct file_offset_mapping* shared_executable;
  struct file_backed file_backed_executable;
  struct swappable_page freestanding;
};

struct page_common {
  enum page_source_type type;
  union page_body body;
};

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
  return type == SHARED_EXECUTABLE;
}

static inline struct page_common init_file_backed(struct file_page_node* file_page) {
  struct page_common ret = {
    .type = FILE_BACKED, 
    .body = {
      .file_backed = {
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

static inline struct page_common init_shared_executable(struct file_offset_mapping* shared_executable) {
  struct page_common ret = {
    .type = SHARED_EXECUTABLE, 
    .body = {
      .shared_executable = shared_executable
    }
  };
  return ret;
}

static inline struct page_common init_file_backed_executable(struct file_page_node* file_page) {
  struct page_common ret = {
    .type = FILE_BACKED_EXECUTABLE, 
    .body = {
      .file_backed_executable = {
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