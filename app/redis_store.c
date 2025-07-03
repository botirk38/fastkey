#include "redis_store.h"
#include "stream.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define LOAD_FACTOR_THRESHOLD 0.75

static uint64_t hash(const char *key) {
  uint64_t hash = 5381;
  int c;
  while ((c = *key++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

RedisStore *createStore(void) {
  RedisStore *store = malloc(sizeof(RedisStore));
  if (!store) {
    return NULL;
  }
  store->size = INITIAL_STORE_SIZE;
  store->used = 0;
  store->table = calloc(store->size, sizeof(StoreEntry *));
  if (!store->table) {
    free(store);
    return NULL;
  }
  if (pthread_rwlock_init(&store->rwlock, NULL) != 0) {
    free(store->table);
    free(store);
    return NULL;
  }
  return store;
}

static void freeEntry(StoreEntry *entry) {
  free(entry->key);
  if (entry->type == TYPE_STRING) {
    free(entry->value.string.data);
  } else if (entry->type == TYPE_STREAM) {
    freeStream(entry->value.stream);
  }
  free(entry);
}

static void resize(RedisStore *store) {
  size_t newSize = store->size * 2;
  StoreEntry **newTable = calloc(newSize, sizeof(StoreEntry *));

  for (size_t i = 0; i < store->size; i++) {
    StoreEntry *entry = store->table[i];
    while (entry) {
      StoreEntry *next = entry->next;
      uint64_t hashVal = hash(entry->key) % newSize;
      entry->next = newTable[hashVal];
      newTable[hashVal] = entry;
      entry = next;
    }
  }

  free(store->table);
  store->table = newTable;
  store->size = newSize;
}

int storeSet(RedisStore *store, const char *key, void *value, size_t valueLen) {
  if (!store || !key || !value) {
    return STORE_ERR;
  }

  pthread_rwlock_wrlock(&store->rwlock);

  if ((float)store->used / store->size > LOAD_FACTOR_THRESHOLD) {
    resize(store);
  }

  uint64_t hashVal = hash(key) % store->size;
  StoreEntry *entry = store->table[hashVal];

  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      if (entry->type == TYPE_STRING) {
        free(entry->value.string.data);
      } else if (entry->type == TYPE_STREAM) {
        freeStream(entry->value.stream);
      }
      entry->type = TYPE_STRING;
      entry->value.string.data = malloc(valueLen);
      if (!entry->value.string.data) {
        pthread_rwlock_unlock(&store->rwlock);
        return STORE_ERR;
      }
      memcpy(entry->value.string.data, value, valueLen);
      entry->value.string.len = valueLen;
      pthread_rwlock_unlock(&store->rwlock);
      return STORE_OK;
    }
    entry = entry->next;
  }

  entry = malloc(sizeof(StoreEntry));
  if (!entry) {
    pthread_rwlock_unlock(&store->rwlock);
    return STORE_ERR;
  }
  entry->key = strdup(key);
  if (!entry->key) {
    free(entry);
    pthread_rwlock_unlock(&store->rwlock);
    return STORE_ERR;
  }
  entry->type = TYPE_STRING;
  entry->value.string.data = malloc(valueLen);
  if (!entry->value.string.data) {
    free(entry->key);
    free(entry);
    pthread_rwlock_unlock(&store->rwlock);
    return STORE_ERR;
  }
  memcpy(entry->value.string.data, value, valueLen);
  entry->value.string.len = valueLen;
  entry->expiry = 0;
  entry->next = store->table[hashVal];
  store->table[hashVal] = entry;
  store->used++;

  pthread_rwlock_unlock(&store->rwlock);
  return STORE_OK;
}

void *storeGet(RedisStore *store, const char *key, size_t *valueLen) {
  if (!store || !key || !valueLen) {
    return NULL;
  }

  pthread_rwlock_rdlock(&store->rwlock);

  uint64_t hashVal = hash(key) % store->size;
  StoreEntry *entry = store->table[hashVal];

  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      if (entry->expiry && entry->expiry <= getCurrentTimeMs()) {
        pthread_rwlock_unlock(&store->rwlock);
        return NULL;
      }
      if (entry->type != TYPE_STRING) {
        pthread_rwlock_unlock(&store->rwlock);
        return NULL;
      }
      void *value = malloc(entry->value.string.len);
      if (!value) {
        pthread_rwlock_unlock(&store->rwlock);
        return NULL;
      }
      memcpy(value, entry->value.string.data, entry->value.string.len);
      *valueLen = entry->value.string.len;
      pthread_rwlock_unlock(&store->rwlock);
      return value;
    }
    entry = entry->next;
  }
  pthread_rwlock_unlock(&store->rwlock);
  return NULL;
}

ValueType getValueType(RedisStore *store, const char *key) {
  uint64_t hashVal = hash(key) % store->size;
  StoreEntry *entry = store->table[hashVal];

  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      if (entry->expiry && entry->expiry < time(NULL)) {
        return TYPE_NONE;
      }
      return entry->type;
    }
    entry = entry->next;
  }
  return TYPE_NONE;
}

int setExpiry(RedisStore *store, const char *key, time_t expiry) {
  uint64_t hashVal = hash(key) % store->size;
  StoreEntry *entry = store->table[hashVal];

  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      entry->expiry = expiry;
      return STORE_OK;
    }
    entry = entry->next;
  }
  return STORE_ERR;
}

void clearExpired(RedisStore *store) {
  time_t now = time(NULL);
  for (size_t i = 0; i < store->size; i++) {
    StoreEntry **entryPtr = &store->table[i];
    while (*entryPtr) {
      if ((*entryPtr)->expiry && (*entryPtr)->expiry < now) {
        StoreEntry *expired = *entryPtr;
        *entryPtr = expired->next;
        freeEntry(expired);
        store->used--;
      } else {
        entryPtr = &(*entryPtr)->next;
      }
    }
  }
}

char *storeStreamAdd(RedisStore *store, const char *key, const char *id,
                     char **fields, char **values, size_t numFields) {
  uint64_t hashVal = hash(key) % store->size;
  StoreEntry *entry = store->table[hashVal];

  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      break;
    }
    entry = entry->next;
  }

  if (!entry) {
    entry = malloc(sizeof(StoreEntry));
    entry->key = strdup(key);
    entry->type = TYPE_STREAM;
    entry->value.stream = createStream();
    entry->next = store->table[hashVal];
    store->table[hashVal] = entry;
    store->used++;
  }

  return streamAdd(entry->value.stream, id, fields, values, numFields);
}

Stream *storeGetStream(RedisStore *store, const char *key) {
  uint64_t hashVal = hash(key) % store->size;
  StoreEntry *entry = store->table[hashVal];

  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      if (entry->type == TYPE_STREAM) {
        return entry->value.stream;
      }
      return NULL;
    }
    entry = entry->next;
  }

  return NULL;
}

time_t getCurrentTimeMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

size_t storeSize(RedisStore *store) {
  if (!store) {
    return 0;
  }
  return store->used;
}

void freeStore(RedisStore *store) {
  if (!store) {
    return;
  }
  
  for (size_t i = 0; i < store->size; i++) {
    StoreEntry *entry = store->table[i];
    while (entry) {
      StoreEntry *next = entry->next;
      freeEntry(entry);
      entry = next;
    }
  }
  free(store->table);
  pthread_rwlock_destroy(&store->rwlock);
  free(store);
}
