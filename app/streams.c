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

Result xadd(KeyValueStore *store, const char *key, const char *id,
            const char **fields, int numFields) {

  Stream *stream = findOrCreateStream(store, strdup(key));

  if (!stream) {
    return (Result){false, "-ERR Failed to create stream"};
  }

  char *generatedID = NULL;
  if (strchr(id, '*')) {
    long long newMillis;
    if (sscanf(id, "%lld-*", &newMillis) != 1) {
      return (Result){false, "-ERR Invalid ID format"};
    }
    generatedID = autoGenerateID(stream, newMillis);
    if (!generatedID) {
      return (Result){false, "-ERR Failed to generate an ID"};
    }
    id = generatedID;
  }

  char *error = validateEntryID(stream, id);

  if (error) {
    fprintf(stderr, "%s", error);
    free(generatedID);
    return (Result){false, error};
  }

  // Check if the stream's current capacity needs to be increased
  if (stream->numEntries == stream->maxEntries) {
    int newCapacity = stream->maxEntries + 10;
    StreamEntry *newEntries =
        realloc(stream->entries, newCapacity * sizeof(StreamEntry));
    if (!newEntries) {
      fprintf(stderr, "Failed to reallocate memory for stream entries\n");
      return (Result){false,
                      "-ERR Failed to reallocate memory for stream entries"};
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
    free(generatedID);

    return (Result){false, "-ERR Failed to allocate memory for entry fields"};
  }

  // Populate the fields of the entry
  for (int i = 0; i < numFields;
       i += 2) { // Assume fields are in key-value pairs
    entry->fields[i / 2].key = strdup(fields[i]);
    entry->fields[i / 2].value = strdup(fields[i + 1]);
  }

  stream->numEntries++;

  free(generatedID);

  return (Result){true, "OK"};
}

int parseEntryID(const char *id, long long *milliseconds, int *sequence) {

  return sscanf(id, "%lld-%d", milliseconds, sequence);
}

char *autoGenerateID(Stream *stream, long long timePart) {
  int newSeq = 0;

  if (stream->numEntries > 0) {
    StreamEntry *lastEntry = &stream->entries[stream->numEntries - 1];
    long long lastMillis;
    int lastSeq;
    parseEntryID(lastEntry->id, &lastMillis, &lastSeq);

    if (timePart == lastMillis) {
      newSeq = lastSeq + 1;
    } else {
      newSeq = 0;
    }
  } else if (timePart == 0) {
    newSeq = 1; // Start at 1 if no existing entries with a zero timestamp
  }

  char *newID = malloc(30);
  if (!newID) {
    perror("Failed to allocate memory for new ID");
    return NULL;
  }

  sprintf(newID, "%lld-%d", timePart, newSeq);
  return newID;
}

char *validateEntryID(Stream *stream, const char *id) {
  long long newMillis, lastMillis;
  int newSeq, lastSeq;

  printf("ID: %s\n", id);

  if (parseEntryID(id, &newMillis, &newSeq) != 2) {
    return "-ERR Invalid ID format\r\n";
  }

  printf("New millis: %lld, new seq: %d\n", newMillis, newSeq);

  if (stream->numEntries > 0) {

    StreamEntry *lastEntry = &stream->entries[stream->numEntries - 1];
    parseEntryID(lastEntry->id, &lastMillis, &lastSeq);

    printf("Last millis: %lld, last seq: %d\n", lastMillis, lastSeq);

    if (newMillis == 0 && newSeq == 0) {
      return "-ERR The ID specified in XADD must be greater than 0-0\r\n";
    }

    if (newMillis < lastMillis ||
        (newMillis == lastMillis && newSeq <= lastSeq)) {
      return "-ERR The ID specified in XADD is equal or smaller than the "
             "target stream top item\r\n";
    }
  } else {
    printf("No entries\n");

    if (newMillis == 0 && newSeq == 0) {
      return "-ERR The ID specified in XADD must be greater than 0-0\r\n";
    }
  }

  return NULL;
}
