#ifndef PARSER_H

#define PARSER_H

#include "./utils/utils.h"
#include <stdbool.h>


typedef struct {
  char *command;
  char **args;
  int numArgs;
} RespCommand;

RespCommand *parseCommand(char *buffer);
bool tryParseCommand(char* buffer);
bool isWriteCommand(const char* command);
void freeRespCommand(RespCommand *respCommand);

#endif // !PARSER_H
