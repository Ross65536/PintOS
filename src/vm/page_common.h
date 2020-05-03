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

struct swappable_page {
  block_sector_t swap_number;
};

struct file_backed_executable {
  bool file_loaded;
  struct file_page_node* file;
  struct swappable_page swap;
};

union page_body {
  struct file_page_node* file_backed;
  struct file_offset_mapping* shared_executable;
  struct file_backed_executable file_backed_executable;
  struct swappable_page freestanding;
};

struct page_common {
  enum page_source_type type;
  union page_body body;
};



static inline struct page_common init_file_backed(struct file_page_node* file_page) {
  struct page_common ret = {
    .type = FILE_BACKED, 
    .body = {
      .file_backed = file_page
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
        .file_loaded = false,
        .file = file_page,
        .swap = {
          .swap_number = -1
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