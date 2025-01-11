#ifndef RESP_H
#define RESP_H

#include "stream.h"
#include <stddef.h>

/* Response status codes */
#define RESP_OK 0             /* Operation completed successfully */
#define RESP_ERR -1           /* Operation failed */
#define RESP_INCOMPLETE 1     /* More data needed to complete parsing */
#define RESP_BUFFER_SIZE 4096 /* Initial buffer size for RESP parsing */

/**
 * RESP protocol value types
 */
typedef enum {
  RespTypeString,  /* Simple string response (+) */
  RespTypeError,   /* Error response (-) */
  RespTypeInteger, /* Integer response (:) */
  RespTypeBulk,    /* Bulk string response ($) */
  RespTypeArray    /* Array response (*) */
} RespType;

/**
 * Represents a RESP protocol value
 */
typedef struct RespValue {
  RespType type;
  union {
    struct {
      char *str;  /* String data */
      size_t len; /* String length */
    } string;
    long long integer; /* Integer value */
    struct {
      struct RespValue **elements; /* Array elements */
      size_t len;                  /* Array length */
    } array;
  } data;
} RespValue;

typedef struct {
  const char *key;
  StreamEntry *entries;
  size_t count;
} StreamInfo;

/**
 * Buffer for accumulating and parsing RESP data
 */
typedef struct RespBuffer {
  char *buffer; /* Raw data buffer */
  size_t size;  /* Total buffer size */
  size_t used;  /* Amount of buffer currently used */
} RespBuffer;

/**
 * Creates a new RESP buffer
 * @return Newly allocated RespBuffer or NULL on failure
 */
RespBuffer *createRespBuffer(void);

/**
 * Frees a RESP buffer and its contents
 * @param buffer Buffer to free
 */
void freeRespBuffer(RespBuffer *buffer);

/**
 * Appends data to a RESP buffer
 * @param buffer Buffer to append to
 * @param data Data to append
 * @param len Length of data
 * @return RESP_OK on success, RESP_ERR on failure
 */
int appendRespBuffer(RespBuffer *buffer, const char *data, size_t len);

/**
 * Parses RESP protocol data from buffer
 * @param buffer Buffer containing RESP data
 * @param value Pointer to store parsed value
 * @return RESP_OK on success, RESP_ERR on failure, RESP_INCOMPLETE if more data
 * needed
 */
int parseResp(RespBuffer *buffer, RespValue **value);

/**
 * Frees a RESP value and all its contents
 * @param value Value to free
 */
void freeRespValue(RespValue *value);

/**
 * Encodes a string as a RESP bulk string
 * @param str String to encode
 * @param len Length of string
 * @param outputLen Pointer to store output length
 * @return Newly allocated string containing RESP bulk string
 */
char *encodeBulkString(const char *str, size_t len, size_t *outputLen);

char *createSimpleString(const char *str);
char *createError(const char *message);
char *createInteger(long long num);
char *createBulkString(const char *str, size_t len);
char *createNullBulkString(void);
char *createRespArray(const char **elements, size_t count);
char *createRespArrayFromElements(RespValue **elements, size_t count);
char *createRespNestedArray(const char ***arrays, size_t *arraySizes,
                            size_t arrayCount);

char *createXrangeResponse(StreamEntry *entries, size_t count);
char *createXreadResponse(StreamInfo *streams, size_t numStreams);
RespValue *cloneRespValue(RespValue *original);
#endif
