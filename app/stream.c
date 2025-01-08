#include "stream.h"
#include <stdlib.h>
#include <string.h>

static const bool parseStreamId(const char *id, StreamID *parsed);

Stream *createStream(void) {
  Stream *stream = malloc(sizeof(Stream));
  stream->head = NULL;
  stream->tail = NULL;
  return stream;
}

char *streamAdd(Stream *stream, const char *id, char **fields, char **values,
                size_t numFields) {

  StreamID parsedId;
  if (!parseStreamId(id, &parsedId)) {
    return NULL;
  }

  if (!isValidNextID(stream, &parsedId)) {
    if (parsedId.ms == 0 && parsedId.seq == 0) {
      return strdup(
          "-ERR The ID specified in XADD must be greater than 0-0\r\n");
    }
    return strdup("-ERR The ID specified in XADD is equal or smaller than the "
                  "target stream top item\r\n");
  }

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

static const bool parseStreamId(const char *id, StreamID *parsed) {

  const char *dash = strchr(id, '-');

  if (!dash) {
    return false;
  }

  char *endMs;
  char *endSeq;
  parsed->ms = strtoull(id, &endMs, 10);
  parsed->seq = strtoull(dash + 1, &endSeq, 10);

  return endMs == dash && *endSeq == '\0';
}

bool isValidNextID(Stream *stream, const StreamID *newId) {
  // Check minimum valid ID
  if (newId->ms == 0 && newId->seq == 0) {
    return false;
  }

  // Empty stream - any valid ID is acceptable
  if (!stream->tail) {
    return true;
  }

  StreamID lastId;
  if (!parseStreamId(stream->tail->id, &lastId)) {
    return false;
  }

  // New ID must be greater
  if (newId->ms < lastId.ms) {
    return false;
  }
  if (newId->ms == lastId.ms && newId->seq <= lastId.seq) {
    return false;
  }

  return true;
}
