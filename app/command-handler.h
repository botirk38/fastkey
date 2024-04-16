#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "utils/KeyValueStore.h"

// Function pointer type for handling commands with multiple arguments.
typedef char *(*CommandHandler)(char **args);

// Struct to represent a command and its associated handler.
typedef struct {
  const char *command;
  CommandHandler handler;
} Command;

char *handleCommand(const char *command, char **args);

char *handlePing(char **args);
char *handleEcho(char **args);
char *handleSet(char **args);
char *handleGet(char **args);

extern Command commandTable[];
extern KeyValueStore store;

#endif // COMMAND_HANDLER_H

