#ifndef PARSER_H

#define PARSER_H

#include "./utils/utils.h"

typedef struct {
  char *command;
  char **args;
  int numArgs;
} RespCommand;

RespCommand *parseCommand(char *buffer);
void freeRespCommand(RespCommand *respCommand);

#endif // !PARSER_H
