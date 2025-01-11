#include "command_queue.h"
#include <stdlib.h>

CommandQueue *createCommandQueue(void) {
  CommandQueue *queue = malloc(sizeof(CommandQueue));
  queue->commands = NULL;
  queue->size = 0;
  queue->capacity = 0;
  return queue;
}

void queueCommand(CommandQueue *queue, RespValue *command) {
  if (queue->size >= queue->capacity) {
    size_t new_capacity = queue->capacity == 0 ? 4 : queue->capacity * 2;
    queue->commands =
        realloc(queue->commands, new_capacity * sizeof(RespValue *));
    queue->capacity = new_capacity;
  }
  queue->commands[queue->size++] = cloneRespValue(command);
}

void clearCommandQueue(CommandQueue *queue) {
  for (size_t i = 0; i < queue->size; i++) {
    freeRespValue(queue->commands[i]);
  }
  free(queue->commands);
  queue->commands = NULL;
  queue->size = 0;
  queue->capacity = 0;
}

void freeCommandQueue(CommandQueue *queue) {
  clearCommandQueue(queue);
  free(queue);
}
