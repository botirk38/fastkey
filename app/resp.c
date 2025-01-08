#define _GNU_SOURCE
#include "resp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RespBuffer *createRespBuffer() {
  RespBuffer *respBuffer = malloc(sizeof(RespBuffer));
  respBuffer->buffer = malloc(RESP_BUFFER_SIZE);
  respBuffer->size = RESP_BUFFER_SIZE;
  respBuffer->used = 0;
  return respBuffer;
}

void freeRespBuffer(RespBuffer *buffer) {
  free(buffer->buffer);
  free(buffer);
}

int appendRespBuffer(RespBuffer *buffer, const char *data, size_t len) {
  /* Resize buffer if needed */
  if (buffer->used + len > buffer->size) {
    size_t newSize = buffer->size * 2;
    while (newSize < buffer->used + len) {
      newSize *= 2;
    }
    char *newBuffer = realloc(buffer->buffer, newSize);
    if (!newBuffer)
      return RESP_ERR;
    buffer->buffer = newBuffer;
    buffer->size = newSize;
  }

  memcpy(buffer->buffer + buffer->used, data, len);
  buffer->used += len;
  return RESP_OK;
}

/**
 * Helper function to parse a line ending in CRLF
 * @return RESP_OK on success, RESP_INCOMPLETE if no CRLF found
 */
static int parseLine(const char *data, size_t len, size_t *lineLen,
                     char **line) {
  char *crlf = memmem(data, len, "\r\n", 2);
  if (!crlf)
    return RESP_INCOMPLETE;

  *lineLen = crlf - data;
  *line = malloc(*lineLen + 1);
  memcpy(*line, data, *lineLen);
  (*line)[*lineLen] = '\0';
  return RESP_OK;
}

/**
 * Creates a new RESP string value
 */
static RespValue *createRespString(const char *str, size_t len) {
  RespValue *value = malloc(sizeof(RespValue));
  value->type = RespTypeString;
  value->data.string.str = malloc(len + 1);
  memcpy(value->data.string.str, str, len);
  value->data.string.str[len] = '\0';
  value->data.string.len = len;
  return value;
}

/**
 * Parses a RESP bulk string
 * Format: $<length>\r\n<data>\r\n
 */
static int parseBulkString(const char *data, size_t len, RespValue **value,
                           size_t *consumed) {
  char *line;
  size_t lineLen;

  if (parseLine(data, len, &lineLen, &line) != RESP_OK) {
    return RESP_INCOMPLETE;
  }

  int strLen = atoi(line + 1); /* Skip '$' */
  free(line);

  *consumed = lineLen + 2; /* Include CRLF */
  if (strLen < 0)
    return RESP_ERR;
  if (len < *consumed + strLen + 2)
    return RESP_INCOMPLETE;

  *value = createRespString(data + *consumed, strLen);
  *consumed += strLen + 2; /* Include final CRLF */

  return RESP_OK;
}

/**
 * Parses a RESP array
 * Format: *<length>\r\n<element-1>...<element-n>
 */
static int parseArray(const char *data, size_t len, RespValue **value,
                      size_t *consumed) {
  char *line;
  size_t lineLen;

  if (parseLine(data, len, &lineLen, &line) != RESP_OK) {
    return RESP_INCOMPLETE;
  }

  int arrayLen = atoi(line + 1); /* Skip '*' */
  free(line);

  *consumed = lineLen + 2; /* Include CRLF */
  if (arrayLen < 0)
    return RESP_ERR;

  RespValue *arrayValue = malloc(sizeof(RespValue));
  arrayValue->type = RespTypeArray;
  arrayValue->data.array.len = arrayLen;
  arrayValue->data.array.elements = malloc(sizeof(RespValue *) * arrayLen);

  data += *consumed;
  len -= *consumed;

  for (int i = 0; i < arrayLen; i++) {
    size_t elementConsumed;
    int result = parseBulkString(data, len, &arrayValue->data.array.elements[i],
                                 &elementConsumed);
    if (result != RESP_OK) {
      freeRespValue(arrayValue);
      return result;
    }
    data += elementConsumed;
    len -= elementConsumed;
    *consumed += elementConsumed;
  }

  *value = arrayValue;
  return RESP_OK;
}

int parseResp(RespBuffer *buffer, RespValue **value) {
  if (buffer->used < 3)
    return RESP_INCOMPLETE;

  size_t consumed;
  int result;

  switch (buffer->buffer[0]) {
  case '*':
    result = parseArray(buffer->buffer, buffer->used, value, &consumed);
    break;
  case '$':
    result = parseBulkString(buffer->buffer, buffer->used, value, &consumed);
    break;
  default:
    return RESP_ERR;
  }

  if (result == RESP_OK) {
    memmove(buffer->buffer, buffer->buffer + consumed, buffer->used - consumed);
    buffer->used -= consumed;
  }

  return result;
}

void freeRespValue(RespValue *value) {
  if (!value)
    return;

  switch (value->type) {
  case RespTypeString:
  case RespTypeBulk:
  case RespTypeError:
    free(value->data.string.str);
    break;
  case RespTypeArray:
    for (size_t i = 0; i < value->data.array.len; i++) {
      freeRespValue(value->data.array.elements[i]);
    }
    free(value->data.array.elements);
    break;
  case RespTypeInteger:
    break; /* Nothing to free for integers */
  }
  free(value);
}

char *encodeBulkString(const char *str, size_t len, size_t *outputLen) {
  char header[32];
  int headerLen = snprintf(header, sizeof(header), "$%zu\r\n", len);

  *outputLen = headerLen + len + 2;
  char *output = malloc(*outputLen + 1); // Add space for null terminator

  memcpy(output, header, headerLen);
  memcpy(output + headerLen, str, len);
  memcpy(output + headerLen + len, "\r\n", 2);
  output[*outputLen] = '\0'; // Null terminate the string

  return output;
}
