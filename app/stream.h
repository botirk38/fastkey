#ifndef STREAM_H
#define STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct StreamID {
  uint64_t ms;  // milliseconds time
  uint64_t seq; // sequence number
} StreamID;

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
StreamEntry *streamRange(Stream *stream, const char *start, const char *end,
                         size_t *count);
StreamEntry *streamRead(Stream *stream, const char *id, size_t *count);

void freeStreamEntry(StreamEntry *entry);

// Stream IDs

bool validateStreamID(const char *id, StreamID *parsed);
bool isValidNextID(Stream *stream, const StreamID *newId);
bool parseStreamID(const char *id, StreamID *parsed);
uint64_t getNextSequence(Stream *stream, uint64_t ms);
char *generateStreamID(uint64_t ms, uint64_t seq);

#endif
