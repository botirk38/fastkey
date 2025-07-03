#define _GNU_SOURCE
#include "test_framework.h"
#include "stream.h"
#include <string.h>
#include <stdlib.h>

void test_create_stream(void) {
    Stream *stream = createStream();
    TEST_ASSERT_NOT_NULL(stream, "Stream creation should succeed");
    TEST_ASSERT_NULL(stream->head, "Stream head should be NULL initially");
    TEST_ASSERT_NULL(stream->tail, "Stream tail should be NULL initially");
    freeStream(stream);
}

void test_stream_add_entry(void) {
    Stream *stream = createStream();
    char *fields[] = {"field1", "field2"};
    char *values[] = {"value1", "value2"};
    
    char *id = streamAdd(stream, "1234567890123-0", fields, values, 2);
    TEST_ASSERT_NOT_NULL(id, "Stream add should return an ID");
    TEST_ASSERT_STRING_EQUAL("1234567890123-0", id, "Returned ID should match input");
    
    TEST_ASSERT_NOT_NULL(stream->head, "Stream head should not be NULL after adding");
    TEST_ASSERT_NOT_NULL(stream->tail, "Stream tail should not be NULL after adding");
    TEST_ASSERT_PTR_EQUAL(stream->head, stream->tail, "Head and tail should be same for single entry");
    
    free(id);
    freeStream(stream);
}

void test_stream_add_multiple_entries(void) {
    Stream *stream = createStream();
    char *fields1[] = {"field1"};
    char *values1[] = {"value1"};
    char *fields2[] = {"field2"};
    char *values2[] = {"value2"};
    
    char *id1 = streamAdd(stream, "1234567890123-0", fields1, values1, 1);
    char *id2 = streamAdd(stream, "1234567890123-1", fields2, values2, 1);
    
    TEST_ASSERT_NOT_NULL(id1, "First stream add should return an ID");
    TEST_ASSERT_NOT_NULL(id2, "Second stream add should return an ID");
    
    TEST_ASSERT_NOT_NULL(stream->head, "Stream head should not be NULL");
    TEST_ASSERT_NOT_NULL(stream->tail, "Stream tail should not be NULL");
    TEST_ASSERT(stream->head != stream->tail, "Head and tail should be different for multiple entries");
    
    free(id1);
    free(id2);
    freeStream(stream);
}

void test_stream_add_auto_id(void) {
    Stream *stream = createStream();
    char *fields[] = {"field1"};
    char *values[] = {"value1"};
    
    char *id = streamAdd(stream, "*", fields, values, 1);
    TEST_ASSERT_NOT_NULL(id, "Stream add with auto ID should return an ID");
    TEST_ASSERT(strlen(id) > 0, "Auto-generated ID should not be empty");
    
    free(id);
    freeStream(stream);
}

void test_stream_range(void) {
    Stream *stream = createStream();
    char *fields[] = {"field1"};
    char *values[] = {"value1"};
    
    streamAdd(stream, "1234567890123-0", fields, values, 1);
    streamAdd(stream, "1234567890124-0", fields, values, 1);
    streamAdd(stream, "1234567890125-0", fields, values, 1);
    
    size_t count;
    StreamEntry *entries = streamRange(stream, "-", "+", &count);
    TEST_ASSERT_NOT_NULL(entries, "Stream range should return entries");
    TEST_ASSERT_EQUAL(3, count, "Stream range should return all entries");
    
    freeStreamEntry(entries);
    freeStream(stream);
}

void test_stream_range_with_bounds(void) {
    Stream *stream = createStream();
    char *fields[] = {"field1"};
    char *values[] = {"value1"};
    
    streamAdd(stream, "1234567890123-0", fields, values, 1);
    streamAdd(stream, "1234567890124-0", fields, values, 1);
    streamAdd(stream, "1234567890125-0", fields, values, 1);
    
    size_t count;
    StreamEntry *entries = streamRange(stream, "1234567890124-0", "1234567890125-0", &count);
    TEST_ASSERT_NOT_NULL(entries, "Stream range with bounds should return entries");
    TEST_ASSERT_EQUAL(2, count, "Stream range should return bounded entries");
    
    freeStreamEntry(entries);
    freeStream(stream);
}

void test_stream_read(void) {
    Stream *stream = createStream();
    char *fields[] = {"field1"};
    char *values[] = {"value1"};
    
    streamAdd(stream, "1234567890123-0", fields, values, 1);
    streamAdd(stream, "1234567890124-0", fields, values, 1);
    
    size_t count;
    StreamEntry *entries = streamRead(stream, "1234567890123-0", &count);
    TEST_ASSERT_NOT_NULL(entries, "Stream read should return entries");
    TEST_ASSERT_EQUAL(1, count, "Stream read should return entries after specified ID");
    
    freeStreamEntry(entries);
    freeStream(stream);
}

void test_validate_stream_id(void) {
    // Note: validateStreamID function not implemented in current codebase
    // This test is disabled until the function is available
    printf("test_validate_stream_id: SKIPPED (function not implemented)\n");
}

void test_parse_stream_id(void) {
    StreamID parsed;
    
    bool success = parseStreamID("1234567890123-456", &parsed);
    TEST_ASSERT(success, "Valid stream ID should be parsed");
    TEST_ASSERT_EQUAL(1234567890123, parsed.ms, "Milliseconds should be parsed correctly");
    TEST_ASSERT_EQUAL(456, parsed.seq, "Sequence should be parsed correctly");
    
    success = parseStreamID("0-0", &parsed);
    TEST_ASSERT(success, "Zero stream ID should be parsed");
    TEST_ASSERT_EQUAL(0, parsed.ms, "Zero milliseconds should be parsed");
    TEST_ASSERT_EQUAL(0, parsed.seq, "Zero sequence should be parsed");
    
    success = parseStreamID("invalid", &parsed);
    TEST_ASSERT(!success, "Invalid stream ID should not be parsed");
}

void test_generate_stream_id(void) {
    char *id = generateStreamID(1234567890123, 456);
    TEST_ASSERT_NOT_NULL(id, "Stream ID generation should succeed");
    TEST_ASSERT_STRING_EQUAL("1234567890123-456", id, "Generated ID should match expected format");
    free(id);
    
    id = generateStreamID(0, 0);
    TEST_ASSERT_NOT_NULL(id, "Zero stream ID generation should succeed");
    TEST_ASSERT_STRING_EQUAL("0-0", id, "Zero ID should match expected format");
    free(id);
}

void run_stream_tests(void) {
    printf("\n=== Stream Tests ===\n");
    RUN_TEST(test_create_stream);
    RUN_TEST(test_stream_add_entry);
    RUN_TEST(test_stream_add_multiple_entries);
    RUN_TEST(test_stream_add_auto_id);
    RUN_TEST(test_stream_range);
    RUN_TEST(test_stream_range_with_bounds);
    RUN_TEST(test_stream_read);
    // RUN_TEST(test_validate_stream_id); // Skipped - function not implemented
    RUN_TEST(test_parse_stream_id);
    RUN_TEST(test_generate_stream_id);
}