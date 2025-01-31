#include "command.h"
#include "event_loop.h"
#include "rdb.h"
#include "redis_store.h"
#include "replicas.h"
#include "resp.h"
#include "server.h"
#include "stream.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static WaitState wait_state = {.remaining_count = 0,
                               .mutex = PTHREAD_MUTEX_INITIALIZER,
                               .condition = PTHREAD_COND_INITIALIZER,
                               .acks_received = 0,
                               .completed = false};

static void *wait_thread(void *arg) {
  WaitState *state = (WaitState *)arg;

  printf("[WAIT-THREAD] Starting wait thread with timeout %d ms\n",
         state->timeout_ms);

  pthread_mutex_lock(&state->mutex);
  printf("Locked mutex\n");

  struct timespec wait_until;
  clock_gettime(CLOCK_REALTIME, &wait_until);

  // Add 100ms buffer to the timeout
  int timeout_with_buffer = state->timeout_ms + 100;

  // Calculate timeout properly with buffer
  wait_until.tv_sec += timeout_with_buffer / 1000;
  wait_until.tv_nsec += (timeout_with_buffer % 1000) * 1000000L;

  // Handle nanosecond overflow
  if (wait_until.tv_nsec >= 1000000000L) {
    wait_until.tv_sec++;
    wait_until.tv_nsec -= 1000000000L;
  }

  printf("[WAIT-THREAD] Waiting until %ld.%ld\n", wait_until.tv_sec,
         wait_until.tv_nsec);

  while (!state->completed && state->acks_received < state->remaining_count) {
    int result =
        pthread_cond_timedwait(&state->condition, &state->mutex, &wait_until);
    if (result == ETIMEDOUT) {
      printf("[WAIT-THREAD] Timeout reached\n");
      break;
    }
  }

  state->completed = true;
  pthread_mutex_unlock(&state->mutex);
  printf("[WAIT-THREAD] Thread completed\n");

  return NULL;
}

static const char *handleSet(RedisServer *server, RedisStore *store,
                             RespValue *command, ClientState *clientState) {
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

static const char *handleGet(RedisServer *server, RedisStore *store,
                             RespValue *command, ClientState *clientState) {
  RespValue *key = command->data.array.elements[1];

  // Check in-memory store first
  size_t memValueLen;
  void *memValue = storeGet(store, key->data.string.str, &memValueLen);
  if (memValue) {
    char *response = createBulkString(memValue, memValueLen);
    free(memValue);
    return response;
  }

  // Try reading from RDB file
  RdbReader *reader = createRdbReader(server->dir, server->filename);
  if (!reader) {
    return createNullBulkString();
  }

  // Get value from RDB
  size_t rdbValueLen;
  char *rdbValue = getRdbValue(reader, key->data.string.str, &rdbValueLen);
  freeRdbReader(reader);

  if (rdbValue) {
    char *response = createBulkString(rdbValue, rdbValueLen);
    free(rdbValue);
    return response;
  }

  return createNullBulkString();
}

static const char *handleType(RedisServer *server, RedisStore *store,
                              RespValue *command, ClientState *clientState) {
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

static const char *handleEcho(RedisServer *server, RedisStore *store,
                              RespValue *command, ClientState *clientState) {
  RespValue *message = command->data.array.elements[1];
  return createBulkString(message->data.string.str, message->data.string.len);
}

static const char *handlePing(RedisServer *server, RedisStore *store,
                              RespValue *command, ClientState *clientState) {
  return createSimpleString("PONG");
}

static const char *handleXadd(RedisServer *server, RedisStore *store,
                              RespValue *command, ClientState *clientState) {
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

static const char *handleXrange(RedisServer *server, RedisStore *store,
                                RespValue *command, ClientState *clientState) {
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

static const char *handleXread(RedisServer *server, RedisStore *store,
                               RespValue *command, ClientState *clientState) {
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

static const char *handleIncrement(RedisServer *server, RedisStore *store,
                                   RespValue *command,
                                   ClientState *clientState) {
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

static const char *handleMulti(RedisServer *server, RedisStore *store,
                               RespValue *command, ClientState *clientState) {
  clientState->in_transaction = 1;
  return createSimpleString("OK");
}

static const char *handleExec(RedisServer *server, RedisStore *store,
                              RespValue *command, ClientState *clientState) {
  printf("[EXEC] Starting transaction execution\n");

  if (!clientState->in_transaction) {
    printf("[EXEC] Error: Not in transaction\n");
    return createError("ERR EXEC without MULTI");
  }

  printf("[EXEC] Executing %zu queued commands\n", clientState->queue->size);

  // Create array of responses
  RespValue **responses =
      malloc(clientState->queue->size * sizeof(RespValue *));

  clientState->in_transaction = 0;

  // Execute each queued command
  for (size_t i = 0; i < clientState->queue->size; i++) {
    RespValue *cmd = clientState->queue->commands[i];
    printf("[EXEC] Executing command %zu: %s\n", i + 1,
           cmd->data.array.elements[0]->data.string.str);

    const char *response = executeCommand(server, store, cmd, clientState);
    printf("[EXEC] Command response: %s\n", response);

    responses[i] = parseResponseToRespValue(response);
    free((void *)response);
  }

  printf("[EXEC] Creating final response array\n");
  char *result =
      createRespArrayFromElements(responses, clientState->queue->size);

  // Cleanup
  for (size_t i = 0; i < clientState->queue->size; i++) {
    freeRespValue(responses[i]);
  }
  free(responses);

  // Reset transaction state
  clientState->in_transaction = 0;
  clearCommandQueue(clientState->queue);

  printf("[EXEC] Transaction completed successfully\n");
  return result;
}

static const char *handleDiscard(RedisServer *server, RedisStore *store,
                                 RespValue *command, ClientState *client) {
  if (!client->in_transaction) {
    const char *err = createError("ERR DISCARD without MULTI");
    return err;
  }

  clearCommandQueue(client->queue);
  client->in_transaction = false;
  const char *res = createSimpleString("OK");
  return res;
}

static const char *handleConfigGet(RedisServer *server, RedisStore *store,
                                   RespValue *command,
                                   ClientState *clientState) {
  // Get parameter name from command
  RespValue *param = command->data.array.elements[2]; // CONFIG GET param

  // Create array elements for response
  RespValue *elements[2];

  if (strcasecmp(param->data.string.str, "dir") == 0) {
    // Create parameter name element
    elements[0] = createRespString("dir", 3);
    // Create parameter value element
    elements[1] = createRespString(server->dir, strlen(server->dir));

    // Create RESP array response
    char *response = createRespArrayFromElements(elements, 2);

    // Clean up
    freeRespValue(elements[0]);
    freeRespValue(elements[1]);

    return response;
  }

  // For other parameters, return empty array
  return createRespArray(NULL, 0);
}

static const char *handleKeys(RedisServer *server, RedisStore *store,
                              RespValue *command, ClientState *clientState) {
  RespValue *pattern = command->data.array.elements[1];
  if (strcmp(pattern->data.string.str, "*") != 0) {
    return createRespArray(NULL, 0);
  }

  RdbReader *reader = createRdbReader(server->dir, server->filename);
  if (!reader) {
    return createRespArray(NULL, 0);
  }

  size_t keyCount;
  RespValue **keys = getRdbKeys(reader, &keyCount);
  freeRdbReader(reader);

  if (!keys) {
    return createRespArray(NULL, 0);
  }

  char *result = createRespArrayFromElements(keys, keyCount);

  // Cleanup
  for (size_t i = 0; i < keyCount; i++) {
    freeRespValue(keys[i]);
  }
  free(keys);

  return result;
}

static const char *handleInfo(RedisServer *server, RedisStore *store,
                              RespValue *command, ClientState *clientState) {
  const char *role = server->repl_info->master_info ? "slave" : "master";

  return createFormattedBulkString("role:%s\r\n"
                                   "master_replid:%s\r\n"
                                   "master_repl_offset:%lld",
                                   role, server->repl_info->replication_id,
                                   server->repl_info->repl_offset);
}

static const char *handleReplConf(RedisServer *server, RedisStore *store,
                                  RespValue *command,
                                  ClientState *clientState) {
  RespValue *subcommand = command->data.array.elements[1];

  printf("handleReplConf called with subcommand: %s\n",
         subcommand->data.string.str);

  if (strcasecmp(subcommand->data.string.str, "getack") == 0) {
    printf("Processing GETACK command, current offset: %lld\n",
           server->repl_info->repl_offset);

    // Create response with current offset
    char offset_str[32];
    snprintf(offset_str, sizeof(offset_str), "%lld",
             server->repl_info->repl_offset);

    RespValue *elements[3];
    elements[0] = createRespString("REPLCONF", 8);
    elements[1] = createRespString("ACK", 3);
    elements[2] = createRespString(offset_str, strlen(offset_str));

    printf("Creating GETACK response with offset: %s\n", offset_str);

    char *response = createRespArrayFromElements(elements, 3);

    freeRespValue(elements[0]);
    freeRespValue(elements[1]);
    freeRespValue(elements[2]);

    return response;

  } else if (strcasecmp(subcommand->data.string.str, "ack") == 0) {
    printf("Processing ACK command\n");

    pthread_mutex_lock(&wait_state.mutex);
    printf("Current remaining count: %ld\n", wait_state.remaining_count);

    printf("Wait State not completed\n");
    wait_state.acks_received++;
    wait_state.remaining_count--;
    printf("Incremented acks received to: %zu\n", wait_state.acks_received);

    if (wait_state.acks_received >= wait_state.remaining_count) {
      printf("Required ACKs received, signaling condition\n");
      pthread_cond_signal(&wait_state.condition);
    }
    pthread_mutex_unlock(&wait_state.mutex);
    return NULL;
  }

  printf("Returning OK response\n");
  return createSimpleString("OK");
}

static const char *handlePsync(RedisServer *server, RedisStore *store,
                               RespValue *command, ClientState *clientState) {

  if (server->repl_info->master_info != NULL)
    return NULL;

  static const unsigned char EMPTY_RDB[] = {0x52, 0x45, 0x44, 0x49, 0x53, 0x30,
                                            0x30, 0x30, 0x39, 0xFF, 0x09, 0x0A,
                                            0x40, 0x3F, 0x72, 0x6E, 0x64};

  RespBuffer *buffer = createRespBuffer();

  // Add FULLRESYNC response using helper
  char *fullresync = createFormattedSimpleString(
      "FULLRESYNC %s %lld", server->repl_info->replication_id,
      server->repl_info->repl_offset);
  appendRespBuffer(buffer, fullresync, strlen(fullresync));
  free(fullresync);

  // Add RDB length prefix
  char prefix[32];
  int prefix_len =
      snprintf(prefix, sizeof(prefix), "$%zu\r\n", sizeof(EMPTY_RDB));
  appendRespBuffer(buffer, prefix, prefix_len);

  // Add raw RDB data without \r\n
  appendRespBuffer(buffer, (char *)EMPTY_RDB, sizeof(EMPTY_RDB));

  char *response = strndup(buffer->buffer, buffer->used);
  freeRespBuffer(buffer);

  addReplica(server, clientState->fd);

  return response;
}

static const char *handleWait(RedisServer *server, RedisStore *store,
                              RespValue *command, ClientState *clientState) {
  printf("[WAIT] Starting WAIT command execution\n");

  size_t numreplicas = atoll(command->data.array.elements[1]->data.string.str);
  int timeout = atoll(command->data.array.elements[2]->data.string.str);

  printf("[WAIT] Requested wait for %zu replicas with timeout %d\n",
         numreplicas, timeout);

  if (!server->repl_info->master_info && server->repl_info->repl_offset == 0) {
    return createInteger(server->repl_info->replicas->replica_count);
  }

  // Send REPLCONF GETACK to all replicas
  for (size_t i = 0; i < server->repl_info->replicas->replica_count; i++) {
    RespValue *getack_elements[3];
    getack_elements[0] = createRespString("REPLCONF", 8);
    getack_elements[1] = createRespString("GETACK", 6);
    getack_elements[2] = createRespString("*", 1);

    char *getack_cmd = createRespArrayFromElements(getack_elements, 3);
    int fd = server->repl_info->replicas->replicas[i].fd;

    write(fd, getack_cmd, strlen(getack_cmd));

    for (int j = 0; j < 3; j++) {
      freeRespValue(getack_elements[j]);
    }
    free(getack_cmd);
  }

  pthread_mutex_lock(&wait_state.mutex);
  wait_state.remaining_count = numreplicas;
  wait_state.completed = false;
  wait_state.acks_received = 0;
  wait_state.timeout_ms = timeout;
  pthread_mutex_unlock(&wait_state.mutex);

  pthread_t thread_id;
  pthread_create(&thread_id, NULL, wait_thread, &wait_state);

  pthread_join(thread_id, NULL);

  // Get final acks count

  pthread_mutex_lock(&wait_state.mutex);
  size_t acked = wait_state.acks_received;
  pthread_mutex_unlock(&wait_state.mutex);

  printf("Acked %lu\n,", acked);

  return createInteger(acked);
}

static CommandHandler baseCommands[] = {
    {"SET", handleSet, 3, 5},
    {"GET", handleGet, 2, 2},
    {"PING", handlePing, 1, 1},
    {"ECHO", handleEcho, 2, 2},
    {"TYPE", handleType, 2, 2},
    {"XADD", handleXadd, 4, -1},
    {"XRANGE", handleXrange, 4, 4},
    {"XREAD", handleXread, 4, -1},
    {"INCR", handleIncrement, 2, 2},
    {"MULTI", handleMulti, 1, 1},
    {"EXEC", handleExec, 1, 1},
    {"DISCARD", handleDiscard, 0, -1},
    {"CONFIG", handleConfigGet, 3, 3},
    {
        "KEYS",
        handleKeys,
        2,
        2,
    },

    {"INFO", handleInfo, 1, 2},
    {"REPLCONF", handleReplConf, 3, 3},
    {"PSYNC", handlePsync, 3, 3},
    {"WAIT", handleWait, 3, 3},
};

static const size_t commandCount =
    sizeof(baseCommands) / sizeof(CommandHandler);

const char *executeCommand(RedisServer *server, RedisStore *store,
                           RespValue *command, ClientState *clientState) {
  // Validate command format
  if (command->type != RespTypeArray || command->data.array.len < 1) {
    return createError("wrong number of arguments");
  }

  RespValue *cmdName = command->data.array.elements[0];

  // Find the command handler
  CommandHandler *handler = NULL;
  for (size_t i = 0; i < commandCount; i++) {
    if (strcasecmp(cmdName->data.string.str, baseCommands[i].name) == 0) {
      handler = &baseCommands[i];
      break;
    }
  }

  // Handle unknown commands
  if (!handler) {
    return createError("unknown command");
  }

  // Validate argument count
  if (command->data.array.len < handler->minArgs ||
      (handler->maxArgs != -1 && command->data.array.len > handler->maxArgs)) {
    return createError("wrong number of arguments");
  }

  if (strcasecmp(cmdName->data.string.str, "MULTI") == 0 ||
      strcasecmp(cmdName->data.string.str, "EXEC") == 0 ||
      strcasecmp(cmdName->data.string.str, "DISCARD") == 0) {
    return handler->handler(server, store, command, clientState);
  }

  // Queue commands if in transaction
  if (clientState->in_transaction) {
    queueCommand(clientState->queue, command);
    return createSimpleString("QUEUED");
  }

  // Execute command normally
  const char *result = handler->handler(server, store, command, clientState);

  if (!server->repl_info->master_info &&
          strcasecmp(cmdName->data.string.str, "SET") == 0 ||
      strcasecmp(cmdName->data.string.str, "DEL") == 0 ||
      strcasecmp(cmdName->data.string.str, "INCR") == 0 ||
      strcasecmp(cmdName->data.string.str, "XADD") == 0) {
    printf("Propagating Command %s\n", cmdName->data.string.str);
    propagateCommand(server, command);
  }

  return result;
}
