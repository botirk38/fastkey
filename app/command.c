#include "command.h"
#include "redis_store.h"
#include "resp.h"
#include "stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *handleSet(RedisStore *store, RespValue *command) {
  RespValue *key = command->data.array.elements[1];
  RespValue *value = command->data.array.elements[2];

  storeSet(store, key->data.string.str, value->data.string.str,
           value->data.string.len);

  if (command->data.array.len < 5) {
    return createSimpleString("OK");
  }

  RespValue *option = command->data.array.elements[3];
  if (strcasecmp(option->data.string.str, "px") != 0) {
    return createSimpleString("OK");
  }

  RespValue *pxValue = command->data.array.elements[4];
  long long milliseconds = atoll(pxValue->data.string.str);
  if (milliseconds <= 0) {
    return createSimpleString("OK");
  }

  time_t expiry = getCurrentTimeMs() + milliseconds;
  setExpiry(store, key->data.string.str, expiry);

  return createSimpleString("OK");
}

static const char *handleType(RedisStore *store, RespValue *command) {
  RespValue *key = command->data.array.elements[1];
  ValueType type = getValueType(store, key->data.string.str);

  switch (type) {
  case TYPE_STRING:
    return createSimpleString("string");
  case TYPE_STREAM:
    return createSimpleString("stream");
  case TYPE_NONE:
  default:
    return createSimpleString("none");
  }
}

static const char *handleGet(RedisStore *store, RespValue *command) {
  RespValue *key = command->data.array.elements[1];
  size_t valueLen;
  void *value = storeGet(store, key->data.string.str, &valueLen);

  if (value) {
    char *response = createBulkString(value, valueLen);
    free(value);
    return response;
  }
  return createNullBulkString();
}

static const char *handleEcho(RedisStore *store, RespValue *command) {
  RespValue *message = command->data.array.elements[1];
  return createBulkString(message->data.string.str, message->data.string.len);
}

static const char *handlePing(RedisStore *store, RespValue *command) {
  return createSimpleString("PONG");
}

static const char *handleXadd(RedisStore *store, RespValue *command) {
  RespValue *key = command->data.array.elements[1];
  RespValue *id = command->data.array.elements[2];

  size_t numFields = (command->data.array.len - 3) / 2;
  char **fields = malloc(numFields * sizeof(char *));
  char **values = malloc(numFields * sizeof(char *));

  for (size_t i = 0; i < numFields; i++) {
    fields[i] = command->data.array.elements[3 + i * 2]->data.string.str;
    values[i] = command->data.array.elements[4 + i * 2]->data.string.str;
  }

  char *result = storeStreamAdd(store, key->data.string.str,
                                id->data.string.str, fields, values, numFields);

  free(fields);
  free(values);

  if (result[0] == '-') {
    char *response = createError(result + 1);
    free(result);
    return response;
  }

  char *response = createBulkString(result, strlen(result));
  free(result);
  return response;
}

static const char *handleXrange(RedisStore *store, RespValue *command) {
  RespValue *key = command->data.array.elements[1];
  RespValue *start = command->data.array.elements[2];
  RespValue *end = command->data.array.elements[3];

  Stream *stream = storeGetStream(store, key->data.string.str);
  if (!stream) {
    return createXrangeResponse(NULL, 0);
  }

  size_t count;
  StreamEntry *entries =
      streamRange(stream, start->data.string.str, end->data.string.str, &count);

  char *response = createXrangeResponse(entries, count);

  // Clean up temporary entries
  StreamEntry *current = entries;
  while (current) {
    StreamEntry *next = current->next;
    freeStreamEntry(current);
    current = next;
  }

  return response;
}

static const char *handleXread(RedisStore *store, RespValue *command) {
  int streamsPos = -1;

  // Find STREAMS argument position
  for (size_t i = 1; i < command->data.array.len; i++) {
    if (strcasecmp(command->data.array.elements[i]->data.string.str,
                   "STREAMS") == 0) {
      streamsPos = i;
      break;
    }
  }

  if (streamsPos == -1) {
    return createError("ERR syntax error");
  }

  // Calculate number of streams
  size_t numStreams = (command->data.array.len - streamsPos - 1) / 2;

  // Create array of StreamInfo structs
  StreamInfo *streams = malloc(numStreams * sizeof(StreamInfo));

  // Process each stream
  for (size_t i = 0; i < numStreams; i++) {
    RespValue *key = command->data.array.elements[streamsPos + 1 + i];
    RespValue *id =
        command->data.array.elements[streamsPos + 1 + numStreams + i];

    streams[i].key = key->data.string.str;
    Stream *stream = storeGetStream(store, key->data.string.str);

    if (!stream) {
      streams[i].entries = NULL;
      streams[i].count = 0;
      continue;
    }

    streams[i].entries =
        streamRead(stream, id->data.string.str, &streams[i].count);
  }

  // Create response
  char *response = createXreadResponse(streams, numStreams);

  // Clean up
  for (size_t i = 0; i < numStreams; i++) {
    StreamEntry *current = streams[i].entries;
    while (current) {
      StreamEntry *next = current->next;
      freeStreamEntry(current);
      current = next;
    }
  }
  free(streams);

  return response;
}

static const char *handleIncrement(RedisStore *store, RespValue *command) {
  RespValue *key = command->data.array.elements[1];
  size_t valueLen;
  void *value = storeGet(store, key->data.string.str, &valueLen);

  if (value) {
    // Convert string to number and ensure it's null-terminated
    char *numStr = malloc(valueLen + 1);
    memcpy(numStr, value, valueLen);
    numStr[valueLen] = '\0';

    // Check if the string is a valid number
    char *endptr;
    long long numValue = strtoll(numStr, &endptr, 10);

    // If endptr points to anything other than '\0', the string contains
    // non-numeric characters
    if (*endptr != '\0') {
      free(value);
      free(numStr);
      return createError("ERR value is not an integer or out of range");
    }

    free(value);
    free(numStr);

    numValue++;

    char newStr[32];
    snprintf(newStr, sizeof(newStr), "%lld", numValue);
    storeSet(store, key->data.string.str, newStr, strlen(newStr));

    return createInteger(numValue);
  }

  storeSet(store, key->data.string.str, "1", 1);
  return createInteger(1);
}

static const char *handleMulti(RedisStore *store, RespValue *command) {
  return createSimpleString("OK");
}

static CommandHandler baseCommands[] = {
    {"SET", handleSet, 3, 5},        {"GET", handleGet, 2, 2},
    {"PING", handlePing, 1, 1},      {"ECHO", handleEcho, 2, 2},
    {"TYPE", handleType, 2, 2},      {"XADD", handleXadd, 4, -1},
    {"XRANGE", handleXrange, 4, 4},  {"XREAD", handleXread, 4, -1},
    {"INCR", handleIncrement, 2, 2}, {"MULTI", handleMulti, 1, 1}};

static const size_t commandCount =
    sizeof(baseCommands) / sizeof(CommandHandler);

const char *executeCommand(RedisStore *store, RespValue *command) {
  if (command->type != RespTypeArray || command->data.array.len < 1) {
    return createError("wrong number of arguments");
  }

  RespValue *cmdName = command->data.array.elements[0];

  for (size_t i = 0; i < commandCount; i++) {
    CommandHandler *handler = &baseCommands[i];
    if (strcasecmp(cmdName->data.string.str, handler->name) == 0) {
      if (command->data.array.len < handler->minArgs ||
          (handler->maxArgs != -1 &&
           command->data.array.len > handler->maxArgs)) {
        return createError("wrong number of arguments");
      }
      return handler->handler(store, command);
    }
  }

  return createError("unknown command");
}
