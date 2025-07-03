#define _GNU_SOURCE
#include "test_framework.h"
#include "server.h"
#include "config.h"
#include "redis_store.h"
#include "resp.h"
#include "networking.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define TEST_PORT 16379
#define TEST_TIMEOUT 5

typedef struct {
    RedisServer *server;
    pthread_t thread;
    int running;
} TestServerData;

void *server_thread(void *arg) {
    TestServerData *data = (TestServerData *)arg;
    
    while (data->running) {
        int clientFd = acceptClient(data->server);
        if (clientFd >= 0) {
            close(clientFd);
        }
        usleep(1000);
    }
    return NULL;
}

int create_test_client_socket(void) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TEST_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

void test_server_create_and_init(void) {
    ServerConfig *config = malloc(sizeof(ServerConfig));
    config->port = TEST_PORT;
    config->dir = strdup("/tmp");
    config->dbfilename = strdup("test.rdb");
    config->bindaddr = strdup("127.0.0.1");
    config->master_host = NULL;
    config->master_port = 0;
    config->is_replica = false;
    
    RedisServer *server = createServer(config);
    TEST_ASSERT_NOT_NULL(server, "Server creation should succeed");
    TEST_ASSERT_EQUAL(TEST_PORT, server->port, "Server port should match config");
    TEST_ASSERT_NOT_NULL(server->db, "Server database should be initialized");
    
    int result = initServer(server);
    TEST_ASSERT_EQUAL(0, result, "Server initialization should succeed");
    TEST_ASSERT(server->fd >= 0, "Server socket should be valid");
    
    freeServer(server);
}

void test_server_accept_connection(void) {
    ServerConfig *config = malloc(sizeof(ServerConfig));
    config->port = TEST_PORT + 1;
    config->dir = strdup("/tmp");
    config->dbfilename = strdup("test.rdb");
    config->bindaddr = strdup("127.0.0.1");
    config->master_host = NULL;
    config->master_port = 0;
    config->is_replica = false;
    
    RedisServer *server = createServer(config);
    TEST_ASSERT_NOT_NULL(server, "Server creation should succeed");
    
    int result = initServer(server);
    TEST_ASSERT_EQUAL(0, result, "Server initialization should succeed");
    
    TestServerData server_data = {server, 0, 1};
    pthread_create(&server_data.thread, NULL, server_thread, &server_data);
    
    usleep(100000);
    
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TEST_PORT + 1);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    int connect_result = connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    TEST_ASSERT_EQUAL(0, connect_result, "Client should be able to connect to server");
    
    close(client_sock);
    server_data.running = 0;
    pthread_join(server_data.thread, NULL);
    
    freeServer(server);
}

void test_server_database_operations(void) {
    ServerConfig *config = malloc(sizeof(ServerConfig));
    config->port = TEST_PORT;
    config->dir = strdup("/tmp");
    config->dbfilename = strdup("test.rdb");
    config->bindaddr = strdup("127.0.0.1");
    config->master_host = NULL;
    config->master_port = 0;
    config->is_replica = false;
    
    RedisServer *server = createServer(config);
    TEST_ASSERT_NOT_NULL(server, "Server creation should succeed");
    TEST_ASSERT_NOT_NULL(server->db, "Server database should be initialized");
    
    int result = storeSet(server->db, "test_key", "test_value", 11);
    TEST_ASSERT_EQUAL(STORE_OK, result, "Database set operation should succeed");
    
    size_t valueLen;
    void *value = storeGet(server->db, "test_key", &valueLen);
    TEST_ASSERT_NOT_NULL(value, "Database get operation should succeed");
    TEST_ASSERT_STRING_EQUAL("test_value", (char*)value, "Retrieved value should match");
    
    free(value);
    freeServer(server);
}

void test_server_statistics(void) {
    ServerConfig *config = malloc(sizeof(ServerConfig));
    config->port = TEST_PORT;
    config->dir = strdup("/tmp");
    config->dbfilename = strdup("test.rdb");
    config->bindaddr = strdup("127.0.0.1");
    config->master_host = NULL;
    config->master_port = 0;
    config->is_replica = false;
    
    RedisServer *server = createServer(config);
    TEST_ASSERT_NOT_NULL(server, "Server creation should succeed");
    TEST_ASSERT_EQUAL(0, server->total_commands_processed, "Initial commands processed should be 0");
    TEST_ASSERT_EQUAL(0, server->keyspace_hits, "Initial keyspace hits should be 0");
    TEST_ASSERT_EQUAL(0, server->keyspace_misses, "Initial keyspace misses should be 0");
    
    server->total_commands_processed = 10;
    server->keyspace_hits = 5;
    server->keyspace_misses = 3;
    
    TEST_ASSERT_EQUAL(10, server->total_commands_processed, "Commands processed should be updated");
    TEST_ASSERT_EQUAL(5, server->keyspace_hits, "Keyspace hits should be updated");
    TEST_ASSERT_EQUAL(3, server->keyspace_misses, "Keyspace misses should be updated");
    
    freeServer(server);
}

void test_server_concurrent_operations(void) {
    ServerConfig *config = malloc(sizeof(ServerConfig));
    config->port = TEST_PORT;
    config->dir = strdup("/tmp");
    config->dbfilename = strdup("test.rdb");
    config->bindaddr = strdup("127.0.0.1");
    config->master_host = NULL;
    config->master_port = 0;
    config->is_replica = false;
    
    RedisServer *server = createServer(config);
    TEST_ASSERT_NOT_NULL(server, "Server creation should succeed");
    
    storeSet(server->db, "key1", "value1", 7);
    storeSet(server->db, "key2", "value2", 7);
    storeSet(server->db, "key3", "value3", 7);
    
    size_t valueLen;
    void *value1 = storeGet(server->db, "key1", &valueLen);
    void *value2 = storeGet(server->db, "key2", &valueLen);
    void *value3 = storeGet(server->db, "key3", &valueLen);
    
    TEST_ASSERT_NOT_NULL(value1, "First concurrent get should succeed");
    TEST_ASSERT_NOT_NULL(value2, "Second concurrent get should succeed");
    TEST_ASSERT_NOT_NULL(value3, "Third concurrent get should succeed");
    
    TEST_ASSERT_STRING_EQUAL("value1", (char*)value1, "First value should match");
    TEST_ASSERT_STRING_EQUAL("value2", (char*)value2, "Second value should match");
    TEST_ASSERT_STRING_EQUAL("value3", (char*)value3, "Third value should match");
    
    free(value1);
    free(value2);
    free(value3);
    freeServer(server);
}

void test_server_memory_management(void) {
    ServerConfig *config = malloc(sizeof(ServerConfig));
    config->port = TEST_PORT;
    config->dir = strdup("/tmp");
    config->dbfilename = strdup("test.rdb");
    config->bindaddr = strdup("127.0.0.1");
    config->master_host = NULL;
    config->master_port = 0;
    config->is_replica = false;
    
    RedisServer *server = createServer(config);
    TEST_ASSERT_NOT_NULL(server, "Server creation should succeed");
    
    for (int i = 0; i < 100; i++) {
        char key[32], value[32];
        sprintf(key, "key%d", i);
        sprintf(value, "value%d", i);
        storeSet(server->db, key, value, strlen(value) + 1);
    }
    
    size_t store_size = storeSize(server->db);
    TEST_ASSERT_EQUAL(100, store_size, "Store should contain 100 items");
    
    for (int i = 0; i < 100; i++) {
        char key[32];
        sprintf(key, "key%d", i);
        size_t valueLen;
        void *value = storeGet(server->db, key, &valueLen);
        TEST_ASSERT_NOT_NULL(value, "Each stored value should be retrievable");
        free(value);
    }
    
    freeServer(server);
}

void run_integration_tests(void) {
    printf("\n=== Integration Tests ===\n");
    RUN_TEST(test_server_create_and_init);
    RUN_TEST(test_server_accept_connection);
    RUN_TEST(test_server_database_operations);
    RUN_TEST(test_server_statistics);
    RUN_TEST(test_server_concurrent_operations);
    RUN_TEST(test_server_memory_management);
}