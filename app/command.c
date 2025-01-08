#include "command.h"
#include <stdlib.h>
#include <string.h>

static const char *handleSet(RedisStore *store, RespValue *command) {
  RespValue *key = command->data.array.elements[1];
  RespValue *value = command->data.array.elements[2];

  storeSet(store, key->data.string.str, value->data.string.str,
           value->data.string.len);

  if (command->data.array.len < 5) {
    return strdup("+OK\r\n");
  }

  RespValue *option = command->data.array.elements[3];
  if (strcasecmp(option->data.string.str, "px") != 0) {
    return strdup("+OK\r\n");
  }

  RespValue *pxValue = command->data.array.elements[4];
  long long milliseconds = atoll(pxValue->data.string.str);
  if (milliseconds <= 0) {
    return strdup("+OK\r\n");
  }

  time_t expiry = getCurrentTimeMs() + milliseconds;
  setExpiry(store, key->data.string.str, expiry);

  return strdup("+OK\r\n");
}

static const char *handleGet(RedisStore *store, RespValue *command) {
  RespValue *key = command->data.array.elements[1];
  size_t valueLen;
  void *value = storeGet(store, key->data.string.str, &valueLen);

  if (value) {
    size_t responseLen;
    char *response = encodeBulkString(value, valueLen, &responseLen);
    free(value);
    return response;
  }
  return strdup("$-1\r\n");
}

static const char *handleEcho(RedisStore *store, RespValue *command) {
  RespValue *message = command->data.array.elements[1];
  size_t responseLen;
  return encodeBulkString(message->data.string.str, message->data.string.len,
                          &responseLen);
}

static const char *handlePing(RedisStore *store, RespValue *command) {
  return strdup("+PONG\r\n");
}

static CommandHandler baseCommands[] = {{"SET", handleSet, 3, 3},
                                        {"GET", handleGet, 2, 2},
                                        {"PING", handlePing, 1, 1}};

CommandTable *createCommandTable(void) {
  CommandTable *table = malloc(sizeof(CommandTable));
  size_t commandsSize = sizeof(baseCommands);
  table->commands = malloc(commandsSize);
  memcpy(table->commands, baseCommands, commandsSize);
  table->count = sizeof(baseCommands) / sizeof(CommandHandler);
  return table;
}

void freeCommandTable(CommandTable *table) {
  free(table->commands);
  free(table);
}

const char *executeCommand(RedisStore *store, RespValue *command) {
  if (command->type != RespTypeArray || command->data.array.len < 1) {
    return strdup("-ERR wrong number of arguments\r\n");
  }

  static CommandHandler baseCommands[] = {{"SET", handleSet, 3, 5},
                                          {"GET", handleGet, 2, 2},
                                          {"PING", handlePing, 1, 1},
                                          {"ECHO", handleEcho, 2, 2}};
  static const size_t commandCount =
      sizeof(baseCommands) / sizeof(CommandHandler);

  RespValue *cmdName = command->data.array.elements[0];

  for (size_t i = 0; i < commandCount; i++) {
    CommandHandler *handler = &baseCommands[i];
    if (strcasecmp(cmdName->data.string.str, handler->name) == 0) {
      if (command->data.array.len < handler->minArgs ||
          (handler->maxArgs != -1 &&
           command->data.array.len > handler->maxArgs)) {
        return strdup("-ERR wrong number of arguments\r\n");
      }
      return handler->handler(store, command);
    }
  }

  return strdup("-ERR unknown command\r\n");
}
