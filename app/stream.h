#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>

typedef struct StreamEntry {
  char *id;
  char **fields;
  char **values;
  size_t numFields;
  struct StreamEntry *next;
} StreamEntry;

typedef struct Stream {
  StreamEntry *head;
  StreamEntry *tail;
} Stream;

Stream *createStream(void);
void freeStream(Stream *stream);
char *streamAdd(Stream *stream, const char *id, char **fields, char **values,
                size_t numFields);

#endif
