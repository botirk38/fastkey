#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include "resp.h"

typedef struct {
  RespValue **commands;
  size_t size;
  size_t capacity;
} CommandQueue;

CommandQueue *createCommandQueue(void);
void queueCommand(CommandQueue *queue, RespValue *command);
void clearCommandQueue(CommandQueue *queue);
void freeCommandQueue(CommandQueue *queue);

#endif
