#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "utils/KeyValueStore.h"

// Function pointer type for handling commands with multiple arguments.
typedef char *(*CommandHandler)(char **args, int numArgs);

// Struct to represent a command and its associated handler.
typedef struct {
  const char *command;
  CommandHandler handler;
} Command;

char *handleCommand(const char *command, char **args, int numArgs);

char *handlePing(char **args, int numArgs);
char *handleEcho(char **args, int numArgs);
char *handleSet(char **args, int numArgs);
char *handleGet(char **args, int numArgs);

extern Command commandTable[];
extern KeyValueStore store;

#endif // COMMAND_HANDLER_H

