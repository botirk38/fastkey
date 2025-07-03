#define _GNU_SOURCE
#include "test_framework.h"
#include "resp.h"
#include <string.h>

void test_create_resp_buffer(void) {
    RespBuffer *buffer = createRespBuffer();
    TEST_ASSERT_NOT_NULL(buffer, "RespBuffer creation should succeed");
    TEST_ASSERT_NOT_NULL(buffer->buffer, "Buffer data should not be NULL");
    TEST_ASSERT_EQUAL(RESP_BUFFER_SIZE, buffer->size, "Buffer size should match initial size");
    TEST_ASSERT_EQUAL(0, buffer->used, "Buffer used should be 0 initially");
    freeRespBuffer(buffer);
}

void test_append_resp_buffer(void) {
    RespBuffer *buffer = createRespBuffer();
    const char *data = "hello";
    size_t len = strlen(data);
    
    int result = appendRespBuffer(buffer, data, len);
    TEST_ASSERT_EQUAL(RESP_OK, result, "Append should succeed");
    TEST_ASSERT_EQUAL(len, buffer->used, "Buffer used should match data length");
    TEST_ASSERT(strncmp(buffer->buffer, data, len) == 0, "Buffer content should match appended data");
    
    freeRespBuffer(buffer);
}

void test_append_resp_buffer_resize(void) {
    RespBuffer *buffer = createRespBuffer();
    
    char large_data[RESP_BUFFER_SIZE * 2];
    memset(large_data, 'a', sizeof(large_data) - 1);
    large_data[sizeof(large_data) - 1] = '\0';
    
    int result = appendRespBuffer(buffer, large_data, sizeof(large_data) - 1);
    TEST_ASSERT_EQUAL(RESP_OK, result, "Append large data should succeed");
    TEST_ASSERT(buffer->size >= sizeof(large_data), "Buffer should be resized");
    TEST_ASSERT_EQUAL(sizeof(large_data) - 1, buffer->used, "Buffer used should match large data length");
    
    freeRespBuffer(buffer);
}

void test_create_simple_string(void) {
    const char *str = "OK";
    char *response = createSimpleString(str);
    TEST_ASSERT_NOT_NULL(response, "Simple string creation should succeed");
    TEST_ASSERT_STRING_EQUAL("+OK\r\n", response, "Simple string format should be correct");
    free(response);
}

void test_create_error(void) {
    const char *message = "ERR invalid command";
    char *response = createError(message);
    TEST_ASSERT_NOT_NULL(response, "Error creation should succeed");
    TEST_ASSERT_STRING_EQUAL("-ERR invalid command\r\n", response, "Error format should be correct");
    free(response);
}

void test_create_integer(void) {
    long long num = 42;
    char *response = createInteger(num);
    TEST_ASSERT_NOT_NULL(response, "Integer creation should succeed");
    TEST_ASSERT_STRING_EQUAL(":42\r\n", response, "Integer format should be correct");
    free(response);
    
    num = -123;
    response = createInteger(num);
    TEST_ASSERT_NOT_NULL(response, "Negative integer creation should succeed");
    TEST_ASSERT_STRING_EQUAL(":-123\r\n", response, "Negative integer format should be correct");
    free(response);
}

void test_create_bulk_string(void) {
    const char *str = "hello";
    char *response = createBulkString(str, strlen(str));
    TEST_ASSERT_NOT_NULL(response, "Bulk string creation should succeed");
    TEST_ASSERT_STRING_EQUAL("$5\r\nhello\r\n", response, "Bulk string format should be correct");
    free(response);
}

void test_create_bulk_string_empty(void) {
    const char *str = "";
    char *response = createBulkString(str, strlen(str));
    TEST_ASSERT_NOT_NULL(response, "Empty bulk string creation should succeed");
    TEST_ASSERT_STRING_EQUAL("$0\r\n\r\n", response, "Empty bulk string format should be correct");
    free(response);
}

void test_create_null_bulk_string(void) {
    char *response = createNullBulkString();
    TEST_ASSERT_NOT_NULL(response, "Null bulk string creation should succeed");
    TEST_ASSERT_STRING_EQUAL("$-1\r\n", response, "Null bulk string format should be correct");
    free(response);
}

void test_create_resp_array(void) {
    const char *elements[] = {"hello", "world"};
    char *response = createRespArray(elements, 2);
    TEST_ASSERT_NOT_NULL(response, "RESP array creation should succeed");
    
    const char *expected = "*2\r\n$5\r\nhello\r\n$5\r\nworld\r\n";
    TEST_ASSERT_STRING_EQUAL(expected, response, "RESP array format should be correct");
    free(response);
}

void test_create_resp_array_empty(void) {
    char *response = createRespArray(NULL, 0);
    TEST_ASSERT_NOT_NULL(response, "Empty RESP array creation should succeed");
    TEST_ASSERT_STRING_EQUAL("*0\r\n", response, "Empty RESP array format should be correct");
    free(response);
}

void test_encode_bulk_string(void) {
    const char *str = "test";
    size_t outputLen;
    char *encoded = encodeBulkString(str, strlen(str), &outputLen);
    
    TEST_ASSERT_NOT_NULL(encoded, "Bulk string encoding should succeed");
    TEST_ASSERT_EQUAL(10, outputLen, "Output length should be correct");
    TEST_ASSERT_STRING_EQUAL("$4\r\ntest\r\n", encoded, "Encoded bulk string should be correct");
    free(encoded);
}

void test_create_resp_string(void) {
    const char *str = "hello";
    RespValue *value = createRespString(str, strlen(str));
    TEST_ASSERT_NOT_NULL(value, "RespValue creation should succeed");
    TEST_ASSERT_EQUAL(RespTypeBulk, value->type, "RespValue type should be bulk");
    TEST_ASSERT_EQUAL(strlen(str), value->data.string.len, "String length should match");
    TEST_ASSERT(strncmp(value->data.string.str, str, strlen(str)) == 0, "String content should match");
    freeRespValue(value);
}

void test_parse_line(void) {
    const char *data = "hello\r\nworld\r\n";
    size_t lineLen;
    char *line;
    
    int result = parseLine(data, strlen(data), &lineLen, &line);
    TEST_ASSERT_EQUAL(RESP_OK, result, "Parse line should succeed");
    TEST_ASSERT_EQUAL(5, lineLen, "Line length should be correct");
    TEST_ASSERT(strncmp(line, "hello", 5) == 0, "Line content should be correct");
    free(line);
}

void test_parse_line_incomplete(void) {
    const char *data = "hello";
    size_t lineLen;
    char *line;
    
    int result = parseLine(data, strlen(data), &lineLen, &line);
    TEST_ASSERT_EQUAL(RESP_INCOMPLETE, result, "Parse incomplete line should return RESP_INCOMPLETE");
}

void run_resp_tests(void) {
    printf("\n=== RESP Protocol Tests ===\n");
    RUN_TEST(test_create_resp_buffer);
    RUN_TEST(test_append_resp_buffer);
    RUN_TEST(test_append_resp_buffer_resize);
    RUN_TEST(test_create_simple_string);
    RUN_TEST(test_create_error);
    RUN_TEST(test_create_integer);
    RUN_TEST(test_create_bulk_string);
    RUN_TEST(test_create_bulk_string_empty);
    RUN_TEST(test_create_null_bulk_string);
    RUN_TEST(test_create_resp_array);
    RUN_TEST(test_create_resp_array_empty);
    RUN_TEST(test_encode_bulk_string);
    RUN_TEST(test_create_resp_string);
    RUN_TEST(test_parse_line);
    RUN_TEST(test_parse_line_incomplete);
}