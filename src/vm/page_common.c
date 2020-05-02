
#include <stdio.h>

#include "page_common.h"


static const char* page_source_type_to_string(enum page_source_type type) {
  switch (type) {
    case SHARED_EXECUTABLE:
      return "SHARED_EXECUTABLE";
    case SHARED_FILE_BACKED:
      return "SHARED_FILE_BACKED";
    case FILE_BACKED:
      return "FILE_BACKED";
    case FREESTANDING:
      return "FREESTANDING";
    default:
      PANIC("unknown page type");
  }
}

void print_page_common(struct page_common* page_common) {
  const char* name = page_source_type_to_string(page_common->type);

  printf ("(type=%s, body=", name);

  switch (page_common->type) {
    case SHARED_EXECUTABLE:
      print_file_offset_mapping(page_common->body.shared_executable);
      break;
    case SHARED_FILE_BACKED:
    case FILE_BACKED:
      print_file_page_node(page_common->body.file_backed);
      break;
    case FREESTANDING:
      break;
    default:
      PANIC("unknown page type");
  }

  printf(")");
}