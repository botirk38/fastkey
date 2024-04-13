#ifndef PARSER_H

#define PARSER_H

#include "./utils/utils.h"

typedef struct {
  char *command;
  char *args;
} RespCommand;

RespCommand *parseCommand(char *buffer);

#endif // !PARSER_H

