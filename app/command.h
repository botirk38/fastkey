#ifndef COMMAND_H
#define COMMAND_H

#include "event_loop.h"
#include "redis_store.h"
#include "server.h"

typedef struct {
  const char *name;
  const char *(*handler)(RedisServer *server, RedisStore *, RespValue *,
                         ClientState *);
  int minArgs;
  int maxArgs;
} CommandHandler;

typedef struct CommandTable {
  CommandHandler *commands;
  size_t count;
} CommandTable;

typedef struct {
  size_t remaining_count;
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  size_t acks_received;
  bool completed;
  int timeout_ms;
} WaitState;

const char *executeCommand(RedisServer *server, RedisStore *store,
                           RespValue *command, ClientState *client_state);

#endif
