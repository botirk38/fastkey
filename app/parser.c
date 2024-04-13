#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RespCommand *parseCommand(char *buffer) {

  RespCommand *command = malloc(sizeof(RespCommand));
  if (!command) {
    return NULL;
  }

  // Parsing the total number of elements expected
  int numElements = atoi(buffer + 1);

  buffer = strchr(buffer, '\n') + 1;
  if (!buffer) {
    free(command);
    return NULL;
  }

  if (*buffer != '$') {
    free(command);
    return NULL;
  }

  int commandLength = atoi(buffer + 1);

  if (commandLength <= 0) {
    free(command);
    return NULL;
  }

  buffer = strchr(buffer, '\n') + 1;
  if (!buffer) {
    free(command);
    return NULL;
  }
  command->command = strndup(buffer, commandLength);
  toUpper(command->command);

  buffer += commandLength + 2;

  if (!command->command) {
    free(command);
    return NULL;
  }

  if (numElements > 1) {
    if (*buffer != '$') {
      free(command->command);
      free(command);
      return NULL;
    }

    int argumentLength = atoi(buffer + 1);
    if (argumentLength <= 0) {
      free(command->command);
      free(command);
      return NULL;
    }

    buffer = strchr(buffer, '\n') + 1;
    if (!buffer) {
      free(command->command);
      free(command);
      return NULL;
    }

    command->args = strndup(buffer, argumentLength);
  } else {
    command->args = NULL; // No arguments
  }

  return command;
}

