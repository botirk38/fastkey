#ifndef REDIS_STORE_H
#define REDIS_STORE_H

#include <pthread.h>
#include <stdint.h>
#include <time.h>

#define STORE_OK 0
#define STORE_ERR -1
#define INITIAL_STORE_SIZE 16

typedef struct StoreEntry {
  char *key;
  void *value;
  size_t valueLen;
  struct StoreEntry *next;
  time_t expiry;
} StoreEntry;

typedef struct RedisStore {
  StoreEntry **table;
  size_t size;
  size_t used;
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

// Utility functions
size_t storeSize(RedisStore *store);
void storeClear(RedisStore *store);

#endif
