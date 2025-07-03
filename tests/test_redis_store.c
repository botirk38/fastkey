#define _GNU_SOURCE
#include "test_framework.h"
#include "redis_store.h"
#include <string.h>
#include <unistd.h>
#include <pthread.h>

void test_create_store(void) {
    RedisStore *store = createStore();
    TEST_ASSERT_NOT_NULL(store, "Store creation should succeed");
    TEST_ASSERT_EQUAL(INITIAL_STORE_SIZE, store->size, "Initial store size should be correct");
    TEST_ASSERT_EQUAL(0, store->used, "Initial store usage should be 0");
    freeStore(store);
}

void test_store_set_and_get(void) {
    RedisStore *store = createStore();
    const char *key = "testkey";
    const char *value = "testvalue";
    size_t valueLen = strlen(value) + 1;
    
    int result = storeSet(store, key, (void*)value, valueLen);
    TEST_ASSERT_EQUAL(STORE_OK, result, "Store set should succeed");
    
    size_t retrievedLen;
    void *retrieved = storeGet(store, key, &retrievedLen);
    TEST_ASSERT_NOT_NULL(retrieved, "Retrieved value should not be NULL");
    TEST_ASSERT_EQUAL(valueLen, retrievedLen, "Retrieved value length should match");
    TEST_ASSERT_STRING_EQUAL(value, (char*)retrieved, "Retrieved value should match original");
    
    free(retrieved);
    freeStore(store);
}

void test_store_get_nonexistent(void) {
    RedisStore *store = createStore();
    size_t retrievedLen;
    void *retrieved = storeGet(store, "nonexistent", &retrievedLen);
    TEST_ASSERT_NULL(retrieved, "Non-existent key should return NULL");
    freeStore(store);
}

void test_store_overwrite(void) {
    RedisStore *store = createStore();
    const char *key = "testkey";
    const char *value1 = "value1";
    const char *value2 = "value2";
    size_t valueLen1 = strlen(value1) + 1;
    size_t valueLen2 = strlen(value2) + 1;
    
    storeSet(store, key, (void*)value1, valueLen1);
    storeSet(store, key, (void*)value2, valueLen2);
    
    size_t retrievedLen;
    void *retrieved = storeGet(store, key, &retrievedLen);
    TEST_ASSERT_NOT_NULL(retrieved, "Retrieved value should not be NULL");
    TEST_ASSERT_EQUAL(valueLen2, retrievedLen, "Retrieved value length should match new value");
    TEST_ASSERT_STRING_EQUAL(value2, (char*)retrieved, "Retrieved value should match new value");
    
    free(retrieved);
    freeStore(store);
}

void test_store_multiple_keys(void) {
    RedisStore *store = createStore();
    const char *keys[] = {"key1", "key2", "key3"};
    const char *values[] = {"value1", "value2", "value3"};
    size_t numKeys = 3;
    
    for (size_t i = 0; i < numKeys; i++) {
        size_t valueLen = strlen(values[i]) + 1;
        int result = storeSet(store, keys[i], (void*)values[i], valueLen);
        TEST_ASSERT_EQUAL(STORE_OK, result, "Store set should succeed for multiple keys");
    }
    
    for (size_t i = 0; i < numKeys; i++) {
        size_t retrievedLen;
        void *retrieved = storeGet(store, keys[i], &retrievedLen);
        TEST_ASSERT_NOT_NULL(retrieved, "Retrieved value should not be NULL for multiple keys");
        TEST_ASSERT_STRING_EQUAL(values[i], (char*)retrieved, "Retrieved value should match for multiple keys");
        free(retrieved);
    }
    
    freeStore(store);
}

void test_store_expiry(void) {
    RedisStore *store = createStore();
    const char *key = "expiring_key";
    const char *value = "expiring_value";
    size_t valueLen = strlen(value) + 1;
    
    storeSet(store, key, (void*)value, valueLen);
    
    time_t expiry = getCurrentTimeMs() + 1000; // 1 second from now in milliseconds
    int result = setExpiry(store, key, expiry);
    TEST_ASSERT_EQUAL(STORE_OK, result, "Setting expiry should succeed");
    
    size_t retrievedLen;
    void *retrieved = storeGet(store, key, &retrievedLen);
    TEST_ASSERT_NOT_NULL(retrieved, "Value should still be retrievable before expiry");
    free(retrieved);
    
    sleep(2);
    
    retrieved = storeGet(store, key, &retrievedLen);
    TEST_ASSERT_NULL(retrieved, "Value should be NULL after expiry");
    
    freeStore(store);
}

void test_store_value_type(void) {
    RedisStore *store = createStore();
    const char *key = "test_key";
    const char *value = "test_value";
    size_t valueLen = strlen(value) + 1;
    
    storeSet(store, key, (void*)value, valueLen);
    
    ValueType type = getValueType(store, key);
    TEST_ASSERT_EQUAL(TYPE_STRING, type, "Value type should be TYPE_STRING");
    
    type = getValueType(store, "nonexistent");
    TEST_ASSERT_EQUAL(TYPE_NONE, type, "Non-existent key should return TYPE_NONE");
    
    freeStore(store);
}

void test_store_null_parameters(void) {
    RedisStore *store = createStore();
    const char *key = "test_key";
    const char *value = "test_value";
    size_t valueLen = strlen(value) + 1;
    size_t retrievedLen;
    
    int result = storeSet(NULL, key, (void*)value, valueLen);
    TEST_ASSERT_EQUAL(STORE_ERR, result, "Store set with NULL store should fail");
    
    result = storeSet(store, NULL, (void*)value, valueLen);
    TEST_ASSERT_EQUAL(STORE_ERR, result, "Store set with NULL key should fail");
    
    result = storeSet(store, key, NULL, valueLen);
    TEST_ASSERT_EQUAL(STORE_ERR, result, "Store set with NULL value should fail");
    
    void *retrieved = storeGet(NULL, key, &retrievedLen);
    TEST_ASSERT_NULL(retrieved, "Store get with NULL store should return NULL");
    
    retrieved = storeGet(store, NULL, &retrievedLen);
    TEST_ASSERT_NULL(retrieved, "Store get with NULL key should return NULL");
    
    retrieved = storeGet(store, key, NULL);
    TEST_ASSERT_NULL(retrieved, "Store get with NULL valueLen should return NULL");
    
    freeStore(store);
}

void test_store_hash_collision(void) {
    RedisStore *store = createStore();
    
    char key1[32], key2[32];
    sprintf(key1, "key_collision_1");
    sprintf(key2, "key_collision_2");
    
    const char *value1 = "value1";
    const char *value2 = "value2";
    size_t valueLen1 = strlen(value1) + 1;
    size_t valueLen2 = strlen(value2) + 1;
    
    storeSet(store, key1, (void*)value1, valueLen1);
    storeSet(store, key2, (void*)value2, valueLen2);
    
    size_t retrievedLen;
    void *retrieved1 = storeGet(store, key1, &retrievedLen);
    void *retrieved2 = storeGet(store, key2, &retrievedLen);
    
    TEST_ASSERT_NOT_NULL(retrieved1, "First value should be retrievable");
    TEST_ASSERT_NOT_NULL(retrieved2, "Second value should be retrievable");
    TEST_ASSERT_STRING_EQUAL(value1, (char*)retrieved1, "First retrieved value should match");
    TEST_ASSERT_STRING_EQUAL(value2, (char*)retrieved2, "Second retrieved value should match");
    
    free(retrieved1);
    free(retrieved2);
    freeStore(store);
}

void run_redis_store_tests(void) {
    printf("\n=== Redis Store Tests ===\n");
    RUN_TEST(test_create_store);
    RUN_TEST(test_store_set_and_get);
    RUN_TEST(test_store_get_nonexistent);
    RUN_TEST(test_store_overwrite);
    RUN_TEST(test_store_multiple_keys);
    RUN_TEST(test_store_expiry);
    RUN_TEST(test_store_value_type);
    RUN_TEST(test_store_null_parameters);
    RUN_TEST(test_store_hash_collision);
}