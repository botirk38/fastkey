#include "KeyValueStore.h"
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void initKeyValueStore(KeyValueStore *keyValueStore) {
  keyValueStore->size = 0;
  keyValueStore->store =
      (KeyValue *)malloc(MAX_STORE_LENGTH * sizeof(KeyValue));
  pthread_mutex_init(&keyValueStore->mutex, NULL);
}

void setKeyValue(KeyValueStore *store, const char *key, const char *value,
                 uint64_t expiry) {
  printf("\n=== Starting setKeyValue operation ===\n");
  printf("Input - Key: %s, Value: %s, Expiry: %llu\n", key, value, expiry);

  pthread_mutex_lock(&store->mutex);
  printf("Mutex locked\n");

  if (store->size == MAX_STORE_LENGTH) {
    printf("Error: Store is full (size: %d)\n", store->size);
    pthread_mutex_unlock(&store->mutex);
    return;
  }

  if (strlen(key) > MAX_KEY_LENGTH || strlen(value) > MAX_VALUE_LENGTH) {
    printf("Error: Key or value exceeds maximum length\n");
    printf("Key length: %zu (max: %d)\n", strlen(key), MAX_KEY_LENGTH);
    printf("Value length: %zu (max: %d)\n", strlen(value), MAX_VALUE_LENGTH);
    pthread_mutex_unlock(&store->mutex);
    return;
  }

  printf("Checking for existing key...\n");
  for (int i = 0; i < store->size; i++) {
    if (strcmp(store->store[i].key, key) == 0) {
      printf("Key found at index %d - updating value\n", i);
      printf("Old value: %s\n", (char *)store->store[i].value);
      free((void *)store->store[i].value);
      store->store[i].value = strdup(value);
      store->store[i].expiry = expiry;
      printf("New value set: %s\n", (char *)store->store[i].value);
      pthread_mutex_unlock(&store->mutex);
      printf("Mutex unlocked - Update complete\n");
      return;
    }
  }

  printf("Adding new key-value pair at index %d\n", store->size);
  store->store[store->size].key = strdup(key);
  store->store[store->size].value = strdup(value);
  store->store[store->size].expiry = expiry;
  store->size++;

  printf("New entry added - Current store size: %d\n", store->size);
  pthread_mutex_unlock(&store->mutex);
  printf("Mutex unlocked - Operation complete\n");
  printf("=== setKeyValue operation finished ===\n\n");
}

const char *getKeyValue(KeyValueStore *store, const char *key) {
  printf("\n=== Starting getKeyValue operation ===\n");
  printf("Searching for key: %s\n", key);

  if (!store || !key) {
    printf("Invalid store or key pointer\n");
    printf("=== getKeyValue operation finished with error ===\n\n");
    return NULL;
  }

  printf("Valid store and key\n");

  fflush(stdout);

  printf("Mutex locked\n");
  fflush(stdout);

  const char *result = NULL;

  for (int i = 0; i < store->size; i++) {
    if (!store->store[i].key) {
      printf("Not found\n");
      continue;
    }

    if (strcmp(store->store[i].key, key) == 0) {
      printf("Key found at index %d\n", i);

      if (store->store[i].expiry == 0 ||
          store->store[i].expiry >= currentTime()) {
        result = strdup((const char *)store->store[i].value);
        printf("Value retrieved: %s\n", result);
      } else {
        printf("Key expired\n");
      }
      break;
    }
  }
  fflush(stdout);

  pthread_mutex_unlock(&store->mutex);
  printf("Mutex unlocked\n");
  printf("=== getKeyValue operation finished ===\n\n");
  return result;
}

void deleteKeyValue(KeyValueStore *store, int index) {
  pthread_mutex_lock(&store->mutex);

  if (index < 0 || index >= store->size) {
    pthread_mutex_unlock(&store->mutex);
    return;
  }

  free((void *)store->store[index].key);
  free(store->store[index].value);

  for (int j = index; j < store->size - 1; j++) {
    store->store[j] = store->store[j + 1];
  }

  store->size--;

  pthread_mutex_unlock(&store->mutex);
}

void deleteExpiredKeys(KeyValueStore *store) {
  pthread_mutex_lock(&store->mutex);

  for (int i = 0; i < store->size; i++) {
    if (store->store[i].expiry > 0 && store->store[i].expiry < currentTime()) {
      deleteKeyValue(store, i);
      i--; // Adjust index after deletion
    }
  }

  pthread_mutex_unlock(&store->mutex);
}

void freeKeyValueStore(KeyValueStore *store) {
  pthread_mutex_lock(&store->mutex);

  for (int i = 0; i < store->size; i++) {
    free((void *)store->store[i].key);
    free(store->store[i].value);
  }
  free(store->store);
  store->size = 0;

  pthread_mutex_unlock(&store->mutex);
  pthread_mutex_destroy(&store->mutex);
}

int lengthOfStore(KeyValueStore *store) {
  pthread_mutex_lock(&store->mutex);
  int size = store->size;
  pthread_mutex_unlock(&store->mutex);
  return size;
}

const char *getKeyAtIdx(KeyValueStore *store, int index) {
  pthread_mutex_lock(&store->mutex);

  if (index < 0 || index >= store->size) {
    pthread_mutex_unlock(&store->mutex);
    return NULL;
  }

  const char *key = strdup(store->store[index].key);
  pthread_mutex_unlock(&store->mutex);
  return key;
}

const char *getType(KeyValueStore *store, const char *key) {
  pthread_mutex_lock(&store->mutex);

  for (int i = 0; i < store->size; i++) {
    if (strcmp(store->store[i].key, key) == 0) {
      const char *type;
      switch (store->store[i].type) {
      case STRING:
        type = "+string\r\n";
        break;
      case STREAM:
        type = "+stream\r\n";
        break;
      default:
        type = "+none\r\n";
      }
      pthread_mutex_unlock(&store->mutex);
      return type;
    }
  }

  pthread_mutex_unlock(&store->mutex);
  return "+none\r\n";
}
