#ifndef COMMAND_H
#define COMMAND_H

#include "redis_store.h"
#include "resp.h"

typedef struct CommandHandler {
  const char *name;
  const char *(*handler)(RedisStore *store, RespValue *command);
  int minArgs;
  int maxArgs;
} CommandHandler;

typedef struct CommandTable {
  CommandHandler *commands;
  size_t count;
} CommandTable;

const char *executeCommand(RedisStore *store, RespValue *command);

#endif
