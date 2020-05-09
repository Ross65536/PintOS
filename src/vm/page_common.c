
#include <stdio.h>

#include "page_common.h"


static const char* page_source_type_to_string(enum page_source_type type) {
  switch (type) {
    case SHARED_READONLY_FILE:
      return "SHARED_READONLY_FILE";
    case FILE_BACKED_EXECUTABLE_STATIC:
      return "FILE_BACKED_EXECUTABLE_STATIC";
    case SHARED_WRITABLE_FILE:
      return "SHARED_WRITABLE_FILE";
    case FREESTANDING:
      return "FREESTANDING";
    default:
      PANIC("unknown page type");
  }
}

static void print_swappable_page(struct swappable_page* swap) {
  printf("(swapped=%d, swap_nr=%d)", swap->is_swapped, swap->swap_number);
}

static void print_file_backed(struct file_backed* file) {
  printf("(swap=");
  print_swappable_page(&file->swap);
  printf(", file=");
  print_file_page_node(file->file);
  printf(")");
}

void print_page_common(struct page_common* page_common) {
  const char* name = page_source_type_to_string(page_common->type);

  printf ("(type=%s, body=", name);

  switch (page_common->type) {
    case SHARED_READONLY_FILE:
      print_file_offset_mapping(page_common->body.shared_readonly_file);
      break;
    case FILE_BACKED_EXECUTABLE_STATIC:
      print_file_backed(&page_common->body.file_backed_executable_static);
      break;
    case SHARED_WRITABLE_FILE:
      print_file_backed(&page_common->body.shared_writable_file);
      break;
    case FREESTANDING:
      print_swappable_page(&page_common->body.freestanding);
      break;
    default:
      PANIC("unknown page type");
  }

  printf(")");
}

bool page_common_eq(struct page_common* l, struct page_common* r) {
  if (l->type != r->type) {
    return false;
  }

  switch (l->type) {
    case SHARED_READONLY_FILE:
      return l->body.shared_readonly_file == r->body.shared_readonly_file; 
    case FILE_BACKED_EXECUTABLE_STATIC:
    case SHARED_WRITABLE_FILE:
      PANIC("NOT_IMPLEMENTED");
    case FREESTANDING:
      PANIC("NOT_IMPLEMENTED");
    default:
      PANIC("NOT_IMPLEMENTED");
  }
}