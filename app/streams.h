#ifndef STREAMS_H
#define STREAMS_H


#include "utils/KeyValueStore.h"
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

Stream* findOrCreateStream(KeyValueStore* store,  char *key);
void xadd(KeyValueStore *store,  const char *key, const char* id, const char** fields, int numFields);


#endif // STREAMS_H
