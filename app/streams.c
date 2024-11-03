#include "streams.h"
#include "utils/KeyValueStore.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

pthread_cond_t stream_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t stream_mutex = PTHREAD_MUTEX_INITIALIZER;

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

  if (strcmp(id, "*") == 0) {
    printf("Auto-generating ID\n");
    generatedID = autoGenerateIDFull(stream);
    id = generatedID;
  } else if (strchr(id, '*')) {
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
  entry->numFields = numFields - 1;
  entry->fields = (EntryField *)malloc(sizeof(EntryField) * numFields);

  printf("Num fields: %d\n", numFields);

  if (!entry->fields) {
    perror("Failed to allocate memory for entry fields");
    free(entry->id);
    free(generatedID);

    return (Result){false, "-ERR Failed to allocate memory for entry fields"};
  }

  // Populate the fields of the entry
  for (int i = 0; i < numFields;
       i += 2) { // Assume fields are in key-value pairs
    entry->fields[i].key = strdup(fields[i]);
    entry->fields[i].value = strdup(fields[i + 1]);
  }

  stream->numEntries++;

  free(generatedID);

  return (Result){true, "OK"};
}

char *xread(const char **keys, const char **ids, int numStreams, bool isSlave,
            KeyValueStore *store, int blockTime) {
  // Initialize response buffer
  int bufferSize = 1024;
  char *respArray = malloc(bufferSize);
  if (!respArray) {
    return strdup("-ERR Memory allocation failed\r\n");
  }

  // Track if we found any new entries
  bool foundNewEntries = false;
  struct timespec endTime;
  clock_gettime(CLOCK_REALTIME, &endTime);
  endTime.tv_sec += blockTime / 1000;
  endTime.tv_nsec += (blockTime % 1000) * 1000000;

  bool infiniteBlock = (blockTime == 0);

  while (!foundNewEntries) {
    for (int i = 0; i < numStreams; i++) {
      Stream *stream = findOrCreateStream(store, strdup(keys[i]));
      if (!stream)
        continue;

      long long startMillis;
      int startSeq;
      if (parseEntryID(ids[i], &startMillis, &startSeq) != 2) {
        free(respArray);
        return strdup("-ERR Invalid ID format\r\n");
      }

      // Check for new entries
      for (int j = 0; j < stream->numEntries; j++) {
        StreamEntry entry = stream->entries[j];
        long long entryMillis;
        int entrySeq;
        parseEntryID(entry.id, &entryMillis, &entrySeq);

        if (entryMillis > startMillis ||
            (entryMillis == startMillis && entrySeq > startSeq)) {
          foundNewEntries = true;
          break;
        }
      }
      if (foundNewEntries)
        break;
    }

    if (!foundNewEntries) {
      if (!infiniteBlock && blockTime > 0) {
        usleep(10000); // 10ms sleep
        blockTime -= 10;
        if (blockTime <= 0) {
          free(respArray);
          return strdup("$-1\r\n");
        }
      } else if (infiniteBlock) {
        usleep(10000); // 10ms sleep for infinite blocking
      }
    }
  }

  // Format response with found entries
  char *ptr = respArray;
  ptr += sprintf(ptr, "*%d\r\n", numStreams);

  for (int i = 0; i < numStreams; i++) {
    const char *key = keys[i];
    const char *id = ids[i];
    Stream *stream = findOrCreateStream(store, strdup(key));

    if (!stream) {
      ptr += sprintf(ptr, "*2\r\n$%zu\r\n%s\r\n*0\r\n", strlen(key), key);
      continue;
    }

    long long startMillis;
    int startSeq;
    parseEntryID(id, &startMillis, &startSeq);

    // Count matching entries
    int entryCount = 0;
    for (int j = 0; j < stream->numEntries; j++) {
      StreamEntry entry = stream->entries[j];
      long long entryMillis;
      int entrySeq;
      parseEntryID(entry.id, &entryMillis, &entrySeq);

      if (entryMillis > startMillis ||
          (entryMillis == startMillis && entrySeq > startSeq)) {
        entryCount++;
      }
    }

    // Format stream entries
    ptr += sprintf(ptr, "*2\r\n$%zu\r\n%s\r\n*%d\r\n", strlen(key), key,
                   entryCount);
    for (int j = 0; j < stream->numEntries; j++) {
      StreamEntry entry = stream->entries[j];
      long long entryMillis;
      int entrySeq;
      parseEntryID(entry.id, &entryMillis, &entrySeq);

      if (entryMillis > startMillis ||
          (entryMillis == startMillis && entrySeq > startSeq)) {
        ptr += sprintf(ptr, "*2\r\n$%zu\r\n%s\r\n*%d\r\n", strlen(entry.id),
                       entry.id, entry.numFields * 2);
        for (int k = 0; k < entry.numFields; k++) {
          ptr += sprintf(ptr, "$%zu\r\n%s\r\n$%zu\r\n%s\r\n",
                         strlen(entry.fields[k].key), entry.fields[k].key,
                         strlen(entry.fields[k].value), entry.fields[k].value);
        }
      }
    }
  }

  return respArray;
}

Result xrange(KeyValueStore *store, const char *key, const char *start,
              const char *end) {
  Stream *stream = findOrCreateStream(store, strdup(key));
  if (!stream) {
    return (Result){false, "-ERR Stream not found"};
  }

  long long startMillis = LLONG_MIN, endMillis = LLONG_MAX;
  int startSeq = 0, endSeq = INT_MAX;

  // Handling the start as '-' indicating the beginning of the stream
  if (strcmp(start, "-") != 0) {
    if (parseEntryID(start, &startMillis, &startSeq) != 2) {
      return (Result){false, "-ERR Invalid start ID format"};
    }
  }

  // Parsing the end ID
  if (strcmp(end, "+") != 0) {
    if (parseEntryID(end, &endMillis, &endSeq) != 2) {
      return (Result){false, "-ERR Invalid end ID format"};
    }
  }

  // Check logical order of start and end IDs, unless start is the beginning of
  // the stream
  if ((strcmp(start, "-") != 0 || strcmp(end, "+") != 0) &&
      (startMillis > endMillis ||
       (startMillis == endMillis && startSeq > endSeq))) {
    return (Result){false,
                    "-ERR Start ID must be less than or equal to End ID"};
  }

  // First pass: count the matching entries
  int entryCount = 0;
  for (int i = 0; i < stream->numEntries; i++) {
    StreamEntry entry = stream->entries[i];
    long long entryMillis;
    int entrySeq;
    parseEntryID(entry.id, &entryMillis, &entrySeq);

    if ((strcmp(start, "-") == 0 || entryMillis > startMillis ||
         (entryMillis == startMillis && entrySeq >= startSeq)) &&
        (entryMillis < endMillis ||
         (entryMillis == endMillis && entrySeq <= endSeq))) {
      entryCount++;
    }
  }

  // Allocate memory based on entry count
  int bufferSize =
      1024 *
      entryCount; // Assuming each entry takes up to 1024 bytes on average
  char respArray[bufferSize];
  // Second pass: format the response
  char *ptr = respArray;
  ptr += sprintf(ptr, "*%d\r\n", entryCount);

  for (int i = 0; i < stream->numEntries; i++) {
    StreamEntry entry = stream->entries[i];
    long long entryMillis;
    int entrySeq;
    parseEntryID(entry.id, &entryMillis, &entrySeq);

    if ((strcmp(start, "-") == 0 || entryMillis > startMillis ||
         (entryMillis == startMillis && entrySeq >= startSeq)) &&
        (entryMillis < endMillis ||
         (entryMillis == endMillis && entrySeq <= endSeq))) {
      ptr += sprintf(ptr, "*2\r\n$%zd\r\n%s\r\n*%d\r\n", strlen(entry.id),
                     entry.id, entry.numFields * 2);
      for (int j = 0; j < entry.numFields; j++) {
        ptr += sprintf(ptr, "$%zd\r\n%s\r\n$%zd\r\n%s\r\n",
                       strlen(entry.fields[j].key), entry.fields[j].key,
                       strlen(entry.fields[j].value), entry.fields[j].value);
      }
    }
  }

  return (Result){true, respArray};
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

  snprintf(newID, 30, "%lld-%d", timePart, newSeq);
  return newID;
}

char *autoGenerateIDFull(Stream *stream) {

  long long newMillis = currentTime();
  int newSeq = 0;

  if (stream->numEntries > 0) {
    StreamEntry *lastEntry = &stream->entries[stream->numEntries - 1];
    long long lastMillis;
    int lastSeq;
    parseEntryID(lastEntry->id, &lastMillis, &lastSeq);

    if (newMillis == lastMillis) {
      newSeq = lastSeq + 1;
    } else {
      newSeq = 0;
    }
  }

  char *newID = malloc(30);
  if (!newID) {
    perror("Failed to allocate memory for new ID");
    return NULL;
  }

  snprintf(newID, 30, "%lld-%d", newMillis, newSeq);

  printf("Generated ID: %s\n", newID);

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
