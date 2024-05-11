#ifndef KEY_VALUE_STORE_H
#define KEY_VALUE_STORE_H

#include <stdint.h>
#define MAX_KEY_LENGTH 100
#define MAX_VALUE_LENGTH 100
#define MAX_STORE_LENGTH 1024

typedef enum {
  STRING,
  STREAM,
  NONE,

} DataType;

typedef struct {
  const char *key;
  void *value;
  long long expiry;
  DataType type;
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
const char* getType(KeyValueStore *store, const char* key);

#endif // !DEBUG
