#include "KeyValueStore.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


void initKeyValueStore(KeyValueStore *keyValueStore) {

  keyValueStore->size = 0;
  keyValueStore->store =
      (KeyValue *)malloc(MAX_STORE_LENGTH * sizeof(KeyValue));
}


void setKeyValue(KeyValueStore *store, const char *key, const char *value,
                 uint64_t expiry) {

  if (store->size == MAX_STORE_LENGTH) {
    printf("Store is full\n");
    return;
  }

  if (strlen(key) > MAX_KEY_LENGTH || strlen(value) > MAX_VALUE_LENGTH) {
    printf("Key or value too long\n");
    return;
  }

  printf("Current time: %lld\n", currentTime());

  store->store[store->size].key = key;
  store->store[store->size].value = (void *)value;
  store->store[store->size].expiry = expiry;
  store->size++;

  printf("Setting key: %s, value: %s, expiry: %lld \n,", key, value, expiry);

  printf("Key set\n");
}

const char *getKeyValue(KeyValueStore *store, const char *key) {

  for (int i = 0; i < store->size; i++) {
    if (strcmp(store->store[i].key, key) == 0) {
      printf("Current time GET: %lld\n", currentTime());
      if (store->store[i].expiry == 0 ||
          store->store[i].expiry >= currentTime()) {

        return store->store[i].value;
      } else {
        deleteKeyValue(store, i); // Delete expired key
        return "Key has expired";
      }
    }
  }
  return "Key not found";
}

const char *getKeyAtIdx(KeyValueStore *store, int index) {
  if (index < 0 || index >= store->size) {
    return "Invalid index";
  }
  return store->store[index].key;
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

int lengthOfStore(KeyValueStore *store) { return store->size; }

KeyValue *findKeyValue(KeyValueStore *store, const char *key) {
  for (int i = 0; i < store->size; i++) {
    if (strcmp(store->store[i].key, key) == 0) {
      return &store->store[i];
    }
  }
  return NULL;
}

const char *getType(KeyValueStore *store, const char *key) {

  KeyValue *keyValue = findKeyValue(store, key);
  if (keyValue == NULL) {
    return "+none\r\n";
  }
  switch (keyValue->type) {
  case STRING:
    return "+string\r\n";

  case STREAM:
    return "+stream\r\n";

  default:
    return "+none\r\n";
  }
}
