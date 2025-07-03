#define _GNU_SOURCE
#include "test_framework.h"
#include "command.h"
#include "redis_store.h"
#include "resp.h"
#include "server.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>

RedisServer *create_test_server(void) {
    ServerConfig *config = malloc(sizeof(ServerConfig));
    config->port = 6379;
    config->dir = strdup("/tmp");
    config->dbfilename = strdup("test.rdb");
    config->bindaddr = strdup("127.0.0.1");
    config->master_host = NULL;
    config->master_port = 0;
    config->is_replica = false;
    
    RedisServer *server = createServer(config);
    return server;
}

RespValue *create_test_command(const char **args, size_t argc) {
    RespValue *command = malloc(sizeof(RespValue));
    command->type = RespTypeArray;
    command->data.array.len = argc;
    command->data.array.elements = malloc(argc * sizeof(RespValue*));
    
    for (size_t i = 0; i < argc; i++) {
        command->data.array.elements[i] = createRespString(args[i], strlen(args[i]));
    }
    
    return command;
}

void test_command_ping(void) {
    RedisServer *server = create_test_server();
    RedisStore *store = createStore();
    ClientState client_state = {0};
    
    const char *args[] = {"PING"};
    RespValue *command = create_test_command(args, 1);
    
    const char *response = executeCommand(server, store, command, &client_state);
    TEST_ASSERT_NOT_NULL(response, "PING command should return a response");
    TEST_ASSERT_STRING_EQUAL("+PONG\r\n", response, "PING should return PONG");
    
    freeRespValue(command);
    free((void*)response);
    freeStore(store);
    freeServer(server);
}

void test_command_echo(void) {
    RedisServer *server = create_test_server();
    RedisStore *store = createStore();
    ClientState client_state = {0};
    
    const char *args[] = {"ECHO", "hello world"};
    RespValue *command = create_test_command(args, 2);
    
    const char *response = executeCommand(server, store, command, &client_state);
    TEST_ASSERT_NOT_NULL(response, "ECHO command should return a response");
    TEST_ASSERT_STRING_EQUAL("$11\r\nhello world\r\n", response, "ECHO should return the message");
    
    freeRespValue(command);
    free((void*)response);
    freeStore(store);
    freeServer(server);
}

void test_command_set(void) {
    RedisServer *server = create_test_server();
    RedisStore *store = createStore();
    ClientState client_state = {0};
    
    const char *args[] = {"SET", "key1", "value1"};
    RespValue *command = create_test_command(args, 3);
    
    const char *response = executeCommand(server, store, command, &client_state);
    TEST_ASSERT_NOT_NULL(response, "SET command should return a response");
    TEST_ASSERT_STRING_EQUAL("+OK\r\n", response, "SET should return OK");
    
    size_t valueLen;
    void *value = storeGet(store, "key1", &valueLen);
    TEST_ASSERT_NOT_NULL(value, "Value should be stored");
    TEST_ASSERT_STRING_EQUAL("value1", (char*)value, "Stored value should match");
    
    freeRespValue(command);
    free((void*)response);
    free(value);
    freeStore(store);
    freeServer(server);
}

void test_command_get(void) {
    RedisServer *server = create_test_server();
    RedisStore *store = createStore();
    ClientState client_state = {0};
    
    storeSet(store, "key1", "value1", 6);
    
    const char *args[] = {"GET", "key1"};
    RespValue *command = create_test_command(args, 2);
    
    const char *response = executeCommand(server, store, command, &client_state);
    TEST_ASSERT_NOT_NULL(response, "GET command should return a response");
    TEST_ASSERT_STRING_EQUAL("$6\r\nvalue1\r\n", response, "GET should return the value");
    
    freeRespValue(command);
    free((void*)response);
    freeStore(store);
    freeServer(server);
}

void test_command_get_nonexistent(void) {
    RedisServer *server = create_test_server();
    RedisStore *store = createStore();
    ClientState client_state = {0};
    
    const char *args[] = {"GET", "nonexistent"};
    RespValue *command = create_test_command(args, 2);
    
    const char *response = executeCommand(server, store, command, &client_state);
    TEST_ASSERT_NOT_NULL(response, "GET command should return a response");
    TEST_ASSERT_STRING_EQUAL("$-1\r\n", response, "GET nonexistent should return null");
    
    freeRespValue(command);
    free((void*)response);
    freeStore(store);
    freeServer(server);
}

void test_command_type(void) {
    RedisServer *server = create_test_server();
    RedisStore *store = createStore();
    ClientState client_state = {0};
    
    storeSet(store, "key1", "value1", 7);
    
    const char *args[] = {"TYPE", "key1"};
    RespValue *command = create_test_command(args, 2);
    
    const char *response = executeCommand(server, store, command, &client_state);
    TEST_ASSERT_NOT_NULL(response, "TYPE command should return a response");
    TEST_ASSERT_STRING_EQUAL("+string\r\n", response, "TYPE should return string");
    
    freeRespValue(command);
    free((void*)response);
    freeStore(store);
    freeServer(server);
}

void test_command_type_nonexistent(void) {
    RedisServer *server = create_test_server();
    RedisStore *store = createStore();
    ClientState client_state = {0};
    
    const char *args[] = {"TYPE", "nonexistent"};
    RespValue *command = create_test_command(args, 2);
    
    const char *response = executeCommand(server, store, command, &client_state);
    TEST_ASSERT_NOT_NULL(response, "TYPE command should return a response");
    TEST_ASSERT_STRING_EQUAL("+none\r\n", response, "TYPE nonexistent should return none");
    
    freeRespValue(command);
    free((void*)response);
    freeStore(store);
    freeServer(server);
}

void test_command_set_with_px(void) {
    RedisServer *server = create_test_server();
    RedisStore *store = createStore();
    ClientState client_state = {0};
    
    const char *args[] = {"SET", "key1", "value1", "px", "1000"};
    RespValue *command = create_test_command(args, 5);
    
    const char *response = executeCommand(server, store, command, &client_state);
    TEST_ASSERT_NOT_NULL(response, "SET with PX should return a response");
    TEST_ASSERT_STRING_EQUAL("+OK\r\n", response, "SET with PX should return OK");
    
    size_t valueLen;
    void *value = storeGet(store, "key1", &valueLen);
    TEST_ASSERT_NOT_NULL(value, "Value should be stored");
    TEST_ASSERT_STRING_EQUAL("value1", (char*)value, "Stored value should match");
    
    freeRespValue(command);
    free((void*)response);
    free(value);
    freeStore(store);
    freeServer(server);
}

void test_command_invalid(void) {
    RedisServer *server = create_test_server();
    RedisStore *store = createStore();
    ClientState client_state = {0};
    
    const char *args[] = {"INVALID_COMMAND"};
    RespValue *command = create_test_command(args, 1);
    
    const char *response = executeCommand(server, store, command, &client_state);
    TEST_ASSERT_NOT_NULL(response, "Invalid command should return a response");
    
    freeRespValue(command);
    free((void*)response);
    freeStore(store);
    freeServer(server);
}

void run_command_tests(void) {
    printf("\n=== Command Tests ===\n");
    RUN_TEST(test_command_ping);
    RUN_TEST(test_command_echo);
    RUN_TEST(test_command_set);
    RUN_TEST(test_command_get);
    RUN_TEST(test_command_get_nonexistent);
    RUN_TEST(test_command_type);
    RUN_TEST(test_command_type_nonexistent);
    RUN_TEST(test_command_set_with_px);
    RUN_TEST(test_command_invalid);
}