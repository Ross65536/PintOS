
#include <stdio.h>

#include "page_common.h"


static const char* page_source_type_to_string(enum page_source_type type) {
  switch (type) {
    case SHARED_EXECUTABLE:
      return "SHARED_EXECUTABLE";
    case FILE_BACKED_EXECUTABLE:
      return "FILE_BACKED_EXECUTABLE";
    case FILE_BACKED:
      return "FILE_BACKED";
    case FREESTANDING:
      return "FREESTANDING";
    default:
      PANIC("unknown page type");
  }
}

static void print_swappable_page(struct swappable_page* swap) {
  printf("(swap_nr=%d)", swap->swap_number);
}

void print_page_common(struct page_common* page_common) {
  const char* name = page_source_type_to_string(page_common->type);

  printf ("(type=%s, body=", name);

  switch (page_common->type) {
    case SHARED_EXECUTABLE:
      print_file_offset_mapping(page_common->body.shared_executable);
      break;
    case FILE_BACKED_EXECUTABLE:
      printf("(file_loaded=%d, swap=", page_common->body.file_backed_executable.file_loaded);
      print_swappable_page(&page_common->body.file_backed_executable.swap);
      printf(", file=");
      print_file_page_node(page_common->body.file_backed_executable.file);
      printf(")");
      break;
    case FILE_BACKED:
      print_file_page_node(page_common->body.file_backed);
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
    case SHARED_EXECUTABLE:
      return l->body.shared_executable == r->body.shared_executable; 
    case FILE_BACKED_EXECUTABLE:
    case FILE_BACKED:
      PANIC("NOT_IMPLEMENTED");
    case FREESTANDING:
      PANIC("NOT_IMPLEMENTED");
    default:
      PANIC("NOT_IMPLEMENTED");
  }
}