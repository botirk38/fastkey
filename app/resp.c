#define _GNU_SOURCE
#include "resp.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Buffer Management Implementation
 * ------------------------------
 */

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

/*
 * RESP Protocol Serializer
 * ----------------------
 */

char *createSimpleString(const char *str) {
  size_t len = strlen(str);
  char *response = malloc(len + 4);
  snprintf(response, len + 4, "+%s\r\n", str);
  return response;
}

char *createError(const char *message) {
  size_t len = strlen(message);
  char *response = malloc(len + 4);
  snprintf(response, len + 4, "-%s\r\n", message);
  return response;
}

char *createInteger(long long num) {
  char *response = malloc(32);
  snprintf(response, 32, ":%lld\r\n", num);
  return response;
}

char *encodeBulkString(const char *str, size_t len, size_t *outputLen) {
  char header[32];
  int headerLen = snprintf(header, sizeof(header), "$%zu\r\n", len);
  *outputLen = headerLen + len + 2;
  char *output = malloc(*outputLen + 1);
  memcpy(output, header, headerLen);
  memcpy(output + headerLen, str, len);
  memcpy(output + headerLen + len, "\r\n", 2);
  output[*outputLen] = '\0';
  return output;
}

char *createBulkString(const char *str, size_t len) {
  size_t outputLen;
  return encodeBulkString(str, len, &outputLen);
}

char *createNullBulkString(void) { return strdup("$-1\r\n"); }

char *createRespArray(const char **elements, size_t count) {
  RespBuffer *buffer = createRespBuffer();
  char header[32];
  snprintf(header, sizeof(header), "*%zu\r\n", count);
  appendRespBuffer(buffer, header, strlen(header));

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
  char header[32];
  snprintf(header, sizeof(header), "*%zu\r\n", count);
  appendRespBuffer(buffer, header, strlen(header));

  for (size_t i = 0; i < count; i++) {
    char *elementStr;
    switch (elements[i]->type) {
    case RespTypeString:
      elementStr = createSimpleString(elements[i]->data.string.str);
      break;
    case RespTypeInteger:
      elementStr = createInteger(elements[i]->data.integer);
      break;
    case RespTypeArray:
      elementStr = createRespArrayFromElements(elements[i]->data.array.elements,
                                               elements[i]->data.array.len);
      break;
    case RespTypeBulk:
      elementStr = createBulkString(elements[i]->data.string.str,
                                    elements[i]->data.string.len);
      break;
    case RespTypeError:
      elementStr = createError(elements[i]->data.string.str);
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
  char header[32];
  snprintf(header, sizeof(header), "*%zu\r\n", count);
  appendRespBuffer(buffer, header, strlen(header));

  for (StreamEntry *entry = entries; entry; entry = entry->next) {
    appendRespBuffer(buffer, "*2\r\n", 4);
    char *idStr = createBulkString(entry->id, strlen(entry->id));
    appendRespBuffer(buffer, idStr, strlen(idStr));
    free(idStr);

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

char *createXreadResponse(StreamInfo *streams, size_t numStreams) {
  RespBuffer *buffer = createRespBuffer();
  char header[32];
  snprintf(header, sizeof(header), "*%zu\r\n", numStreams);
  appendRespBuffer(buffer, header, strlen(header));

  for (size_t i = 0; i < numStreams; i++) {
    StreamInfo *streamInfo = &streams[i];
    appendRespBuffer(buffer, "*2\r\n", 4);

    char *keyStr = createBulkString(streamInfo->key, strlen(streamInfo->key));
    appendRespBuffer(buffer, keyStr, strlen(keyStr));
    free(keyStr);

    if (!streamInfo->entries || streamInfo->count == 0) {
      appendRespBuffer(buffer, "*0\r\n", 4);
      continue;
    }

    appendRespBuffer(buffer, "*1\r\n", 4);
    appendRespBuffer(buffer, "*2\r\n", 4);

    char *idStr = createBulkString(streamInfo->entries->id,
                                   strlen(streamInfo->entries->id));
    appendRespBuffer(buffer, idStr, strlen(idStr));
    free(idStr);

    char fieldsHeader[32];
    snprintf(fieldsHeader, sizeof(fieldsHeader), "*%zu\r\n",
             streamInfo->entries->numFields * 2);
    appendRespBuffer(buffer, fieldsHeader, strlen(fieldsHeader));

    for (size_t j = 0; j < streamInfo->entries->numFields; j++) {
      char *fieldStr = createBulkString(streamInfo->entries->fields[j],
                                        strlen(streamInfo->entries->fields[j]));
      char *valueStr = createBulkString(streamInfo->entries->values[j],
                                        strlen(streamInfo->entries->values[j]));
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

// Format Resp funcs

char *createFormattedBulkString(const char *format, ...) {
  char buffer[4096];
  va_list args;

  va_start(args, format);
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  return createBulkString(buffer, len);
}

char *createFormattedSimpleString(const char *format, ...) {
  char buffer[4096];
  va_list args;

  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  return createSimpleString(buffer);
}

/*
 * RESP Protocol Deserializer
 * -------------------------
 */

int parseLine(const char *data, size_t len, size_t *lineLen, char **line) {
  char *crlf = memmem(data, len, "\r\n", 2);
  if (!crlf)
    return RESP_INCOMPLETE;
  *lineLen = crlf - data;
  *line = malloc(*lineLen + 1);
  memcpy(*line, data, *lineLen);
  (*line)[*lineLen] = '\0';
  return RESP_OK;
}

RespValue *createRespString(const char *str, size_t len) {
  RespValue *value = malloc(sizeof(RespValue));
  value->type = RespTypeBulk;
  value->data.string.str = malloc(len + 1);
  memcpy(value->data.string.str, str, len);
  value->data.string.str[len] = '\0';
  value->data.string.len = len;
  return value;
}

static int parseBulkString(const char *data, size_t len, RespValue **value,
                           size_t *consumed) {
  char *line;
  size_t lineLen;
  if (parseLine(data, len, &lineLen, &line) != RESP_OK) {
    return RESP_INCOMPLETE;
  }

  int strLen = atoi(line + 1);
  free(line);
  *consumed = lineLen + 2;

  if (strLen < 0)
    return RESP_ERR;
  if (len < *consumed + strLen + 2)
    return RESP_INCOMPLETE;

  *value = createRespString(data + *consumed, strLen);
  *consumed += strLen + 2;
  return RESP_OK;
}

static int parseArray(const char *data, size_t len, RespValue **value,
                      size_t *consumed) {
  char *line;
  size_t lineLen;
  if (parseLine(data, len, &lineLen, &line) != RESP_OK) {
    return RESP_INCOMPLETE;
  }

  int arrayLen = atoi(line + 1);
  free(line);
  *consumed = lineLen + 2;

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

RespValue *parseResponseToRespValue(const char *response) {
  RespValue *value = malloc(sizeof(RespValue));
  size_t consumed;

  switch (response[0]) {
  case '+':
  case '-': {
    char *line;
    size_t lineLen;
    parseLine(response, strlen(response), &lineLen, &line);
    RespValue *newValue = malloc(sizeof(RespValue));
    newValue->type = (response[0] == '+') ? RespTypeString : RespTypeError;
    newValue->data.string.str = strdup(line + 1);
    newValue->data.string.len = lineLen - 1;
    free(line);
    free(value);
    value = newValue;
    break;
  }
  case ':': {
    char *line;
    size_t lineLen;
    parseLine(response, strlen(response), &lineLen, &line);
    value->type = RespTypeInteger;
    value->data.integer = atoll(line + 1);
    free(line);
    break;
  }
  case '$':
    parseBulkString(response, strlen(response), &value, &consumed);
    break;
  case '*':
    parseArray(response, strlen(response), &value, &consumed);
    break;
  }

  return value;
}

/*
 * Value Management Functions
 * ------------------------
 */

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
    break;
  }
  free(value);
}

RespValue *cloneRespValue(RespValue *original) {
  if (!original)
    return NULL;

  RespValue *clone = malloc(sizeof(RespValue));
  clone->type = original->type;

  switch (original->type) {
  case RespTypeString:
  case RespTypeBulk:
  case RespTypeError:
    clone->data.string.len = original->data.string.len;
    clone->data.string.str = malloc(original->data.string.len + 1);
    memcpy(clone->data.string.str, original->data.string.str,
           original->data.string.len);
    clone->data.string.str[original->data.string.len] = '\0';
    break;

  case RespTypeArray:
    clone->data.array.len = original->data.array.len;
    clone->data.array.elements =
        malloc(sizeof(RespValue *) * original->data.array.len);
    for (size_t i = 0; i < original->data.array.len; i++) {
      clone->data.array.elements[i] =
          cloneRespValue(original->data.array.elements[i]);
    }
    break;

  case RespTypeInteger:
    clone->data.integer = original->data.integer;
    break;
  }

  return clone;
}

size_t calculateCommandSize(RespValue *command) {
  size_t size = 0;

  // Array header
  size += snprintf(NULL, 0, "*%zu\r\n", command->data.array.len);

  // Each element
  for (size_t i = 0; i < command->data.array.len; i++) {
    RespValue *elem = command->data.array.elements[i];
    size += snprintf(NULL, 0, "$%zu\r\n", elem->data.string.len);
    size += elem->data.string.len;
    size += 2; // \r\n
  }

  return size;
}
