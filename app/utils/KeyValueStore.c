#include "KeyValueStore.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void initKeyValueStore(KeyValueStore *keyValueStore) {

  keyValueStore->size = 0;
  keyValueStore->store =
      (KeyValue *)malloc(MAX_STORE_LENGTH * sizeof(KeyValue));
}

int currentTime() { return (int)time(NULL); }

void setKeyValue(KeyValueStore *store, const char *key, const char *value,
                 int expiry) {

  if (store->size == MAX_STORE_LENGTH) {
    printf("Store is full\n");
    return;
  }

  if (strlen(key) > MAX_KEY_LENGTH || strlen(value) > MAX_VALUE_LENGTH) {
    printf("Key or value too long\n");
    return;
  }

  store->store[store->size].key = key;
  store->store[store->size].value = value;
  store->store[store->size].expiry = expiry;
  store->size++;

  printf("Key set\n");
}

const char *getKeyValue(KeyValueStore *store, const char *key) {

  for (int i = 0; i < store->size; i++) {

    if (strcmp(store->store[i].key, key) == 0) {

      if (store->store[i].expiry == 0) {
        return store->store[i].value;
      } else {
        if (store->store[i].expiry > currentTime()) {
          return store->store[i].value;
        } else {
          deleteKeyValue(store, i);
          return "Key has expired";
        }
      }
    }
  }

  return "Key not found";
}

void deleteKeyValue(KeyValueStore *store, int index) {

  if (index < 0 || index >= store->size) {
    printf("Invalid index\n");
    return;
  }

  free((void *)store->store[index].key);
  free((void *)store->store[index].value);

  // Shift elements left to fill the gap
  for (int j = index; j < store->size - 1; j++) {
    store->store[j] = store->store[j + 1];
  }
  store->size--;

  printf("Key not found\n");
}

void deleteExpiredKeys(KeyValueStore *store) {

  for (int i = 0; i < store->size; i++) {

    if (store->store[i].expiry > 0 && store->store[i].expiry < currentTime()) {
      deleteKeyValue(store, i);
    }
  }
}

void freeKeyValueStore(KeyValueStore *store) {

  for (int i = 0; i < store->size; i++) {
    free((void *)store->store[i].key);
    free((void *)store->store[i].value);
  }

  free(store->store);
  store->size = 0;
}
