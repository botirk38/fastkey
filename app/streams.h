#ifndef STREAMS_H
#define STREAMS_H


#include "utils/KeyValueStore.h"
#include <stdbool.h>
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

Stream* findOrCreateStream(KeyValueStore* store,  char *key);
Result xadd(KeyValueStore *store,  const char *key, const char* id, const char** fields, int numFields);
int parseEntryID(const char* id, long long* milliseconds, int* sequence);
char* validateEntryID(Stream *stream, const char* id);
char* autoGenerateID(Stream *stream, long long milliseconds);


#endif // STREAMS_H
