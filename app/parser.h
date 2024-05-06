#ifndef PARSER_H

#define PARSER_H

#include "./utils/utils.h"
#include <stdbool.h>
#include <stdio.h>
#include "utils/KeyValueStore.h"



typedef struct {
  char *command;
  char **args;
  int numArgs;
} RespCommand;

RespCommand *parseCommand(char *buffer);
bool tryParseCommand(char* buffer);
bool isWriteCommand(const char* command);
bool isForbiddenCommand(const char* command);
void freeRespCommand(RespCommand *respCommand);
void parseRDBFile(const char* filename, const char* dir, KeyValueStore* store);
void parseRDBHeader(FILE* file);
void parseStringDataStructure(FILE* file, KeyValueStore* store);
void parseRDBVersion(FILE* file);
void parse_key_value(FILE* file, KeyValueStore* store, uint64_t expiry);

#endif // !PARSER_H
