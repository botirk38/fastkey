#include "stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static uint64_t getCurrentTimeMs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

static bool isIdInRange(const StreamID *id, const StreamID *start,
                        const StreamID *end) {
  if (id->ms < start->ms || id->ms > end->ms) {
    return false;
  }
  if (id->ms == start->ms && id->seq < start->seq) {
    return false;
  }
  if (id->ms == end->ms && id->seq > end->seq) {
    return false;
  }
  return true;
}

Stream *createStream(void) {
  Stream *stream = malloc(sizeof(Stream));
  stream->head = NULL;
  stream->tail = NULL;
  return stream;
}

bool parseStreamID(const char *id, StreamID *parsed) {
  if (strcmp(id, "*") == 0) {
    parsed->ms = getCurrentTimeMs();
    parsed->seq = UINT64_MAX;
    return true;
  }

  char *dash = strchr(id, '-');
  if (!dash) {
    return false;
  }

  if (*(dash + 1) == '*') {
    parsed->ms = strtoull(id, NULL, 10);
    parsed->seq = UINT64_MAX;
    return true;
  }

  parsed->ms = strtoull(id, NULL, 10);
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
  char *id = malloc(32);
  snprintf(id, 32, "%llu-%llu", ms, seq);
  return id;
}

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

char *streamAdd(Stream *stream, const char *id, char **fields, char **values,
                size_t numFields) {
  StreamID parsedId;
  if (!parseStreamID(id, &parsedId)) {
    return NULL;
  }

  if (parsedId.seq == UINT64_MAX) {
    parsedId.seq = getNextSequence(stream, parsedId.ms);
  }

  if (!isValidNextID(stream, &parsedId)) {
    if (parsedId.ms == 0 && parsedId.seq == 0) {
      return strdup("-ERR The ID specified in XADD must be greater than 0-0");
    }
    return strdup("-ERR The ID specified in XADD is equal or smaller than the "
                  "target stream top item");
  }

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

StreamEntry *streamRange(Stream *stream, const char *start, const char *end,
                         size_t *count) {
  StreamID startId, endId;
  parseStreamID(start, &startId);
  parseStreamID(end, &endId);

  StreamEntry *result = NULL;
  StreamEntry *resultTail = NULL;
  *count = 0;

  for (StreamEntry *entry = stream->head; entry; entry = entry->next) {
    StreamID entryId;
    parseStreamID(entry->id, &entryId);

    if (isIdInRange(&entryId, &startId, &endId)) {
      StreamEntry *newEntry = malloc(sizeof(StreamEntry));
      newEntry->id = strdup(entry->id);
      newEntry->numFields = entry->numFields;
      newEntry->fields = malloc(entry->numFields * sizeof(char *));
      newEntry->values = malloc(entry->numFields * sizeof(char *));
      newEntry->next = NULL;

      for (size_t i = 0; i < entry->numFields; i++) {
        newEntry->fields[i] = strdup(entry->fields[i]);
        newEntry->values[i] = strdup(entry->values[i]);
      }

      if (!result) {
        result = newEntry;
      } else {
        resultTail->next = newEntry;
      }
      resultTail = newEntry;
      (*count)++;
    }
  }

  return result;
}

void freeStreamEntry(StreamEntry *entry) {
  free(entry->id);
  for (size_t i = 0; i < entry->numFields; i++) {
    free(entry->fields[i]);
    free(entry->values[i]);
  }
  free(entry->fields);
  free(entry->values);
  free(entry);
}
