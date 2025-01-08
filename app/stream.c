#include "stream.h"
#include <stdlib.h>
#include <string.h>

Stream *createStream(void) {
  Stream *stream = malloc(sizeof(Stream));
  stream->head = NULL;
  stream->tail = NULL;
  return stream;
}

char *streamAdd(Stream *stream, const char *id, char **fields, char **values,
                size_t numFields) {
  StreamEntry *entry = malloc(sizeof(StreamEntry));
  entry->id = strdup(id);
  entry->numFields = numFields;
  entry->fields = malloc(numFields * sizeof(char *));
  entry->values = malloc(numFields * sizeof(char *));
  entry->next = NULL;

  for (size_t i = 0; i < numFields; i++) {
    entry->fields[i] = strdup(fields[i]);
    entry->values[i] = strdup(values[i]);
  }

  if (!stream->head) {
    stream->head = entry;
  } else {
    stream->tail->next = entry;
  }
  stream->tail = entry;

  return strdup(id);
}

void freeStream(Stream *stream) {
  StreamEntry *current = stream->head;
  while (current) {
    StreamEntry *next = current->next;
    free(current->id);
    for (size_t i = 0; i < current->numFields; i++) {
      free(current->fields[i]);
      free(current->values[i]);
    }
    free(current->fields);
    free(current->values);
    free(current);
    current = next;
  }
  free(stream);
}
