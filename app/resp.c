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

char *createSimpleString(const char *str) {
  size_t len = strlen(str);
  char *response = malloc(len + 4); // +3 for "+\r\n" and +1 for null terminator
  snprintf(response, len + 4, "+%s\r\n", str);
  return response;
}

char *createError(const char *message) {
  size_t len = strlen(message);
  char *response = malloc(len + 4); // +3 for "-\r\n" and +1 for null terminator
  snprintf(response, len + 4, "-%s\r\n", message);
  return response;
}

char *createInteger(long long num) {
  char *response = malloc(32); // Enough for any 64-bit integer
  snprintf(response, 32, ":%lld\r\n", num);
  return response;
}

char *createBulkString(const char *str, size_t len) {
  size_t outputLen;
  return encodeBulkString(str, len, &outputLen);
}

char *createNullBulkString(void) { return strdup("$-1\r\n"); }

char *createRespArray(const char **elements, size_t count) {
  RespBuffer *buffer = createRespBuffer();

  // Array header
  char header[32];
  snprintf(header, sizeof(header), "*%zu\r\n", count);
  appendRespBuffer(buffer, header, strlen(header));

  // Elements
  for (size_t i = 0; i < count; i++) {
    char *bulkStr = createBulkString(elements[i], strlen(elements[i]));
    appendRespBuffer(buffer, bulkStr, strlen(bulkStr));
    free(bulkStr);
  }

  char *result = strndup(buffer->buffer, buffer->used);
  freeRespBuffer(buffer);
  return result;
}

char *createRespArrayFromElements(RespValue **elements, size_t count) {
  RespBuffer *buffer = createRespBuffer();

  // Array header
  char header[32];
  snprintf(header, sizeof(header), "*%zu\r\n", count);
  appendRespBuffer(buffer, header, strlen(header));

  for (size_t i = 0; i < count; i++) {
    char *elementStr;
    switch (elements[i]->type) {
    case RespTypeString:
      elementStr = createBulkString(elements[i]->data.string.str,
                                    elements[i]->data.string.len);
      break;
    case RespTypeInteger: {
      char numStr[32];
      snprintf(numStr, sizeof(numStr), "%lld", elements[i]->data.integer);
      elementStr = createBulkString(numStr, strlen(numStr));
      break;
    }
    case RespTypeArray:
      elementStr = createRespArrayFromElements(elements[i]->data.array.elements,
                                               elements[i]->data.array.len);
      break;
    default:
      elementStr = createNullBulkString();
    }
    appendRespBuffer(buffer, elementStr, strlen(elementStr));
    free(elementStr);
  }

  char *result = strndup(buffer->buffer, buffer->used);
  freeRespBuffer(buffer);
  return result;
}

char *createXrangeResponse(StreamEntry *entries, size_t count) {
  if (!entries || count == 0) {
    return createRespArray(NULL, 0);
  }

  RespBuffer *buffer = createRespBuffer();

  // Array header
  char header[32];
  snprintf(header, sizeof(header), "*%zu\r\n", count);
  appendRespBuffer(buffer, header, strlen(header));

  for (StreamEntry *entry = entries; entry; entry = entry->next) {
    // Entry array header
    appendRespBuffer(buffer, "*2\r\n", 4);

    // ID
    char *idStr = createBulkString(entry->id, strlen(entry->id));
    appendRespBuffer(buffer, idStr, strlen(idStr));
    free(idStr);

    // Fields array header
    char fieldsHeader[32];
    snprintf(fieldsHeader, sizeof(fieldsHeader), "*%zu\r\n",
             entry->numFields * 2);
    appendRespBuffer(buffer, fieldsHeader, strlen(fieldsHeader));

    for (size_t i = 0; i < entry->numFields; i++) {
      char *fieldStr =
          createBulkString(entry->fields[i], strlen(entry->fields[i]));
      char *valueStr =
          createBulkString(entry->values[i], strlen(entry->values[i]));
      appendRespBuffer(buffer, fieldStr, strlen(fieldStr));
      appendRespBuffer(buffer, valueStr, strlen(valueStr));
      free(fieldStr);
      free(valueStr);
    }
  }

  char *result = strndup(buffer->buffer, buffer->used);
  freeRespBuffer(buffer);
  return result;
}

char *createXreadResponse(const char *key, StreamEntry *entries, size_t count) {
  if (!entries || count == 0) {
    return strdup("*0\r\n");
  }

  RespBuffer *buffer = createRespBuffer();

  // Build exact format:
  // *1\r\n                     - Top array with 1 element
  // *2\r\n                     - Stream array with key and entries
  // $<keylen>\r\n<key>\r\n    - Stream key
  // *1\r\n                     - Array of entries
  // *2\r\n                     - Entry array (id + fields)
  // $<idlen>\r\n<id>\r\n      - Entry ID
  // *<numfields*2>\r\n        - Fields array
  // $<fieldlen>\r\n<field>\r\n - Field name
  // $<vallen>\r\n<value>\r\n   - Field value

  char header[32];

  appendRespBuffer(buffer, "*1\r\n*2\r\n", 8);

  snprintf(header, sizeof(header), "$%zu\r\n", strlen(key));
  appendRespBuffer(buffer, header, strlen(header));
  appendRespBuffer(buffer, key, strlen(key));
  appendRespBuffer(buffer, "\r\n", 2);

  appendRespBuffer(buffer, "*1\r\n*2\r\n", 8);

  snprintf(header, sizeof(header), "$%zu\r\n", strlen(entries->id));
  appendRespBuffer(buffer, header, strlen(header));
  appendRespBuffer(buffer, entries->id, strlen(entries->id));
  appendRespBuffer(buffer, "\r\n", 2);

  snprintf(header, sizeof(header), "*%zu\r\n", entries->numFields * 2);
  appendRespBuffer(buffer, header, strlen(header));

  for (size_t i = 0; i < entries->numFields; i++) {
    snprintf(header, sizeof(header), "$%zu\r\n", strlen(entries->fields[i]));
    appendRespBuffer(buffer, header, strlen(header));
    appendRespBuffer(buffer, entries->fields[i], strlen(entries->fields[i]));
    appendRespBuffer(buffer, "\r\n", 2);

    snprintf(header, sizeof(header), "$%zu\r\n", strlen(entries->values[i]));
    appendRespBuffer(buffer, header, strlen(header));
    appendRespBuffer(buffer, entries->values[i], strlen(entries->values[i]));
    appendRespBuffer(buffer, "\r\n", 2);
  }

  char *result = strndup(buffer->buffer, buffer->used);
  freeRespBuffer(buffer);
  return result;
}
