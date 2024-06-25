#ifndef STREAMS_H
#define STREAMS_H

#include "utils/KeyValueStore.h"
#include <stdbool.h>

#define MED_BUFFER_SIZE 1024
#define SMALL_BUFFER_SIZE 256

typedef struct {
  char *key;
  char *value;

} EntryField;

typedef struct {
  char *id;
  EntryField *fields;
  int numFields;
} StreamEntry;

typedef struct {
  StreamEntry *entries;
  int numEntries;
  int maxEntries;
} Stream;

typedef struct {
  bool success;
  char *message;

} Result;

Stream *findOrCreateStream(KeyValueStore *store, char *key);
Result xadd(KeyValueStore *store, const char *key, const char *id,
            const char **fields, int numFields);
Result xrange(KeyValueStore *store, const char *key, const char *start,
              const char *end);
char *xread(const char **keys, const char **ids, int numStreams, bool isSlave,
            KeyValueStore *store);
int parseEntryID(const char *id, long long *milliseconds, int *sequence);
char *validateEntryID(Stream *stream, const char *id);
char *autoGenerateID(Stream *stream, long long milliseconds);
char *autoGenerateIDFull(Stream *stream);
int parsePartialID(const char *id, long long *milliseconds, int *sequence);

#endif // STREAMS_H
