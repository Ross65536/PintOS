
#include <kernel/hash.h>
#include <debug.h>
#include <string.h>

#include "threads/malloc.h"
#include "threads/synch.h"
#include "active_files.h"
#include "filesys/off_t.h"

#define MAX_FILEPATH 256

struct file_offset_mapping {
  struct hash_elem elem;
  off_t offset;
  const char* file_path;
  unsigned int process_ref_count;
};

static struct file_offset_mapping* create_file_offset_mapping (const char* file_path, off_t offset) {
  struct file_offset_mapping* mapping = malloc (sizeof (struct file_offset_mapping));
  if (mapping == NULL) {
    return NULL;
  }

  mapping->file_path = file_path;
  mapping->offset = offset;
  mapping->process_ref_count = 0;
  
  return mapping;
}

// active_file

static unsigned int active_file_hash (const struct hash_elem *e, void *_ UNUSED) {
  struct file_offset_mapping *elem = hash_entry (e, struct file_offset_mapping, elem);
  return hash_int (elem->offset);
}

static bool active_file_less (const struct hash_elem *l, const struct hash_elem *r, void *_ UNUSED) {
  struct file_offset_mapping *elem_l = hash_entry (l, struct file_offset_mapping, elem);
  struct file_offset_mapping *elem_r = hash_entry (r, struct file_offset_mapping, elem);

  return elem_l->offset < elem_r->offset;
}

struct active_file {
  struct hash_elem elem;
  char file_path[MAX_FILEPATH];
  struct hash offsets;
};

static struct active_file* create_active_file (const char * file_path) {
  struct active_file* active = malloc (sizeof (struct active_file));
  if (active == NULL) {
    return NULL;
  }

  const bool ok = hash_init(&active->offsets, active_file_hash, active_file_less, NULL);
  if (!ok) {
    free(active);
    return NULL;
  }

  strlcpy(active->file_path, file_path, MAX_FILEPATH);
  return active;
}

static struct file_offset_mapping* find_or_add_file_mapping_offset (struct active_file* active_file, off_t offset) {
  struct file_offset_mapping find_e;
  find_e.offset = offset;

  struct hash_elem* found = hash_find (&active_file->offsets, &find_e.elem);
  if (found != NULL) {
    return hash_entry (found, struct file_offset_mapping, elem);
  }

  struct file_offset_mapping* created = create_file_offset_mapping(active_file->file_path, offset);
  if (created == NULL) {
    return NULL;
  }

  hash_insert (&active_file->offsets, &created->elem);
  return created;
}

// active_files_list

static unsigned int active_files_list_hash (const struct hash_elem *e, void *_ UNUSED) {
  struct active_file *active_file = hash_entry (e, struct active_file, elem);
  return hash_string (active_file->file_path);
}

static bool active_files_list_less (const struct hash_elem *l, const struct hash_elem *r, void *_ UNUSED) {
  struct active_file *active_file_l = hash_entry (l, struct active_file, elem);
  struct active_file *active_file_r = hash_entry (r, struct active_file, elem);

  return strcmp (active_file_l->file_path, active_file_r->file_path) < 0;
}

struct active_files_list {
  struct hash active_files;
  struct lock monitor;
};

static bool init_active_files_list(struct active_files_list* active_files) {
  lock_init (&active_files->monitor);
  return hash_init (&active_files->active_files, active_files_list_hash, active_files_list_less, NULL);
}

static struct active_file* find_or_add_active_file (struct active_files_list* active_files, const char* file_path) {
  struct active_file find_e;
  strlcpy(find_e.file_path, file_path, MAX_FILEPATH);

  struct hash_elem* found = hash_find (&active_files->active_files, &find_e.elem);
  if (found != NULL) {
    return hash_entry (found, struct active_file, elem);
  }

  struct active_file* created = create_active_file(file_path);
  if (created == NULL) {
    return NULL;
  }

  hash_insert (&active_files->active_files, &created->elem);
  return created;
}

////////////
/// impl
////////////

void init_active_files() {
  const bool ok = init_active_files_list(&executable_files);
  ASSERT (ok);
}

struct file_offset_mapping* add_active_file_offset(struct active_files_list* active_files, const char* file_path, off_t offset) {
  lock_acquire (&active_files->monitor);

  struct active_file* file = find_or_add_active_file(active_files, file_path);
  if (file == NULL) {
    lock_release (&active_files->monitor);
    return NULL;
  }

  struct file_offset_mapping* mapping = find_or_add_file_mapping_offset(file, offset);
  if (mapping == NULL) {
    lock_release (&active_files->monitor);
    return NULL;
  }

  mapping->process_ref_count++;
  lock_release (&active_files->monitor);
  return mapping;
}


struct active_files_list executable_files;