#ifndef COMMAND_H
#define COMMAND_H

#include "event_loop.h"
#include "redis_store.h"
#include "resp.h"

typedef struct {
  const char *name;
  const char *(*handler)(RedisStore *, RespValue *, ClientState *);
  int minArgs;
  int maxArgs;
} CommandHandler;

typedef struct CommandTable {
  CommandHandler *commands;
  size_t count;
} CommandTable;

const char *executeCommand(RedisStore *store, RespValue *command,
                           ClientState *client_state);

#endif
