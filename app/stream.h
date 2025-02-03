#ifndef STREAM_H
#define STREAM_H

#include <pthread.h>
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

typedef struct {
  const char *key;
  StreamEntry *entries;
  size_t count;
} StreamInfo;

typedef struct Stream {
  StreamEntry *head;
  StreamEntry *tail;
} Stream;

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  bool has_new_data;
} StreamBlockState;

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

StreamInfo *processStreamReads(Stream **streams, const char **keys,
                               const char **ids, size_t numStreams,
                               bool *hasData);
void freeStreamInfo(StreamInfo *streams, size_t numStreams);
bool waitForStreamData(StreamBlockState *state, int timeoutMs);
StreamInfo *recheckStreams(Stream **streams, const char **keys,
                           const char **ids, size_t numStreams);
StreamBlockState *getStreamBlockState(void);
#endif
