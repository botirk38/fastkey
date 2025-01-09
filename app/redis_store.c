#include "redis_store.h"
#include "stream.h"
#include <stdlib.h>
#include <string.h>

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
  store->size = INITIAL_STORE_SIZE;
  store->used = 0;
  store->table = calloc(store->size, sizeof(StoreEntry *));
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
      memcpy(entry->value.string.data, value, valueLen);
      entry->value.string.len = valueLen;
      return STORE_OK;
    }
    entry = entry->next;
  }

  entry = malloc(sizeof(StoreEntry));
  entry->key = strdup(key);
  entry->type = TYPE_STRING;
  entry->value.string.data = malloc(valueLen);
  memcpy(entry->value.string.data, value, valueLen);
  entry->value.string.len = valueLen;
  entry->expiry = 0;
  entry->next = store->table[hashVal];
  store->table[hashVal] = entry;
  store->used++;

  return STORE_OK;
}

void *storeGet(RedisStore *store, const char *key, size_t *valueLen) {
  uint64_t hashVal = hash(key) % store->size;
  StoreEntry *entry = store->table[hashVal];

  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      if (entry->expiry && entry->expiry <= getCurrentTimeMs()) {
        return NULL;
      }
      if (entry->type != TYPE_STRING) {
        return NULL;
      }
      void *value = malloc(entry->value.string.len);
      memcpy(value, entry->value.string.data, entry->value.string.len);
      *valueLen = entry->value.string.len;
      return value;
    }
    entry = entry->next;
  }
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

void freeStore(RedisStore *store) {
  for (size_t i = 0; i < store->size; i++) {
    StoreEntry *entry = store->table[i];
    while (entry) {
      StoreEntry *next = entry->next;
      freeEntry(entry);
      entry = next;
    }
  }
  free(store->table);
  free(store);
}
