#ifndef REDIS_STORE_H
#define REDIS_STORE_H

#include "stream.h"
#include <pthread.h>
#include <stdint.h>
#include <time.h>

#define STORE_OK 0
#define STORE_ERR -1
#define INITIAL_STORE_SIZE 16

typedef enum ValueType { TYPE_NONE, TYPE_STRING, TYPE_STREAM } ValueType;

typedef struct StoreEntry {
  char *key;
  ValueType type;
  union {
    struct {
      void *data;
      size_t len;
    } string;
    Stream *stream;
  } value;
  struct StoreEntry *next;
  time_t expiry;
} StoreEntry;

typedef struct RedisStore {
  StoreEntry **table;
  size_t size;
  size_t used;
  pthread_rwlock_t rwlock;
} RedisStore;

// Core operations
RedisStore *createStore(void);
void freeStore(RedisStore *store);
int storeSet(RedisStore *store, const char *key, void *value, size_t valueLen);
void *storeGet(RedisStore *store, const char *key, size_t *valueLen);
int storeDelete(RedisStore *store, const char *key);

// Expiry operations
int setExpiry(RedisStore *store, const char *key, time_t expiry);
int getExpiry(RedisStore *store, const char *key, time_t *expiry);
void clearExpired(RedisStore *store);
time_t getCurrentTimeMs(void);

// Type Operations

ValueType getValueType(RedisStore *store, const char *key);

// Stream Operations

char *storeStreamAdd(RedisStore *store, const char *key, const char *id,
                     char **fields, char **values, size_t numFields);
Stream *storeGetStream(RedisStore *store, const char *key);

// Utility functions
size_t storeSize(RedisStore *store);
void storeClear(RedisStore *store);

#endif
