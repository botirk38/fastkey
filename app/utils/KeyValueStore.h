#ifndef KEY_VALUE_STORE_H
#define KEY_VALUE_STORE_H

#include <stdint.h>
#define MAX_KEY_LENGTH 100
#define MAX_VALUE_LENGTH 100
#define MAX_STORE_LENGTH 1024


typedef struct {
  const char *key;
  const char *value;
  long long expiry;
} KeyValue;

typedef struct {
  KeyValue *store;
  int size;

} KeyValueStore;

void initKeyValueStore(KeyValueStore *store);
void setKeyValue(KeyValueStore *store, const char *key, const char *value, uint64_t expiry);
const char *getKeyValue(KeyValueStore *store, const char *key);
void deleteKeyValue(KeyValueStore *store, int index);
long long currentTime();
void deleteExpiredKeys(KeyValueStore *store);
void freeKeyValueStore(KeyValueStore *store);
int lengthOfStore(KeyValueStore *store);
const char* getKeyAtIdx(KeyValueStore *store, int idx);

#endif // !DEBUG
