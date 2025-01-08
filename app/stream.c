#include "stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool isValidNextID(Stream *stream, const StreamID *newId) {
  if (newId->ms == 0 && newId->seq == 0) {
    return false;
  }

  if (!stream->tail) {
    return true;
  }

  StreamID lastId;
  if (!parseStreamID(stream->tail->id, &lastId)) {
    return false;
  }

  if (newId->ms < lastId.ms) {
    return false;
  }
  if (newId->ms == lastId.ms && newId->seq <= lastId.seq) {
    return false;
  }

  return true;
}

bool parseStreamID(const char *id, StreamID *parsed) {
  char *dash = strchr(id, '-');
  if (!dash) {
    return false;
  }

  parsed->ms = strtoull(id, NULL, 10);

  // Handle auto-sequence case
  if (*(dash + 1) == '*') {
    parsed->seq = UINT64_MAX; // Special marker for auto-sequence
    return true;
  }

  parsed->seq = strtoull(dash + 1, NULL, 10);
  return true;
}

uint64_t getNextSequence(Stream *stream, uint64_t ms) {
  if (!stream->tail) {
    return (ms == 0) ? 1 : 0;
  }

  StreamID lastId;
  parseStreamID(stream->tail->id, &lastId);

  if (lastId.ms == ms) {
    return lastId.seq + 1;
  }

  return 0;
}

char *generateStreamID(uint64_t ms, uint64_t seq) {
  char *id = malloc(32); // Enough space for any uint64_t pair
  snprintf(id, 32, "%llu-%llu", ms, seq);
  return id;
}

char *streamAdd(Stream *stream, const char *id, char **fields, char **values,
                size_t numFields) {
  StreamID parsedId;
  if (!parseStreamID(id, &parsedId)) {
    return NULL;
  }

  // Handle auto-sequence
  if (parsedId.seq == UINT64_MAX) {
    parsedId.seq = getNextSequence(stream, parsedId.ms);
  }

  // Validate ID (keeping existing validation)
  if (!isValidNextID(stream, &parsedId)) {
    if (parsedId.ms == 0 && parsedId.seq == 0) {
      return strdup(
          "-ERR The ID specified in XADD must be greater than 0-0\r\n");
    }
    return strdup("-ERR The ID specified in XADD is equal or smaller than the "
                  "target stream top item\r\n");
  }

  // Create new entry with generated ID
  char *finalId = generateStreamID(parsedId.ms, parsedId.seq);
  StreamEntry *entry = malloc(sizeof(StreamEntry));
  entry->id = finalId;
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

  return strdup(finalId);
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

Stream *createStream(void) {
  Stream *stream = malloc(sizeof(Stream));
  stream->head = NULL;
  stream->tail = NULL;
  return stream;
}
