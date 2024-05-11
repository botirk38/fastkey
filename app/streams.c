#include "streams.h"
#include "utils/KeyValueStore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Stream *findOrCreateStream(KeyValueStore *store, char *key) {

  for (int i = 0; i < store->size; i++) {
    if (strcmp(store->store[i].key, key) == 0 &&
        store->store[i].type == STREAM) {
      return store->store[i].value;
    }
  }

  if (store->size == MAX_STORE_LENGTH) {
    fprintf(stderr, "Store is full\n");
    return NULL;
  }

  Stream *stream = malloc(sizeof(Stream));

  if (!stream) {
    perror("Failed to allocate memory for stream");
    return NULL;
  }

  stream->entries = NULL;
  stream->maxEntries = 10;
  stream->numEntries = 0;

  store->store[store->size].key = key;

  stream->entries = malloc(sizeof(StreamEntry) * stream->maxEntries);

  if (!stream->entries) {
    perror("Failed to allocate memory for stream entries");
    free(stream);
    return NULL;
  }

  store->store[store->size].value = stream;
  store->store[store->size].type = STREAM;

  store->size++;

  return stream;
}

void xadd(KeyValueStore *store, const char *key, const char *id,
          const char **fields, int numFields) {

  Stream *stream = findOrCreateStream(store, strdup(key));

  if (!stream) {
    return;
  }

  // Check if the stream's current capacity needs to be increased
  if (stream->numEntries == stream->maxEntries) {
    int newCapacity = stream->maxEntries + 10;
    StreamEntry *newEntries =
        realloc(stream->entries, newCapacity * sizeof(StreamEntry));
    if (!newEntries) {
      fprintf(stderr, "Failed to reallocate memory for stream entries\n");
      return;
    }
    stream->entries = newEntries;
    stream->maxEntries = newCapacity;
  }

  StreamEntry *entry = &stream->entries[stream->numEntries];
  entry->id = strdup(id);
  entry->numFields = numFields;
  entry->fields = (EntryField *)malloc(sizeof(EntryField) * numFields);

  if (!entry->fields) {
    perror("Failed to allocate memory for entry fields");
    free(entry->id);
    return;
  }

  // Populate the fields of the entry
  for (int i = 0; i < numFields;
       i += 2) { // Assume fields are in key-value pairs
    entry->fields[i / 2].key = strdup(fields[i]);
    entry->fields[i / 2].value = strdup(fields[i + 1]);
  }

  stream->numEntries++;
}
