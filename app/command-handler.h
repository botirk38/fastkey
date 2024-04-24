#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "utils/KeyValueStore.h"
#include <stdbool.h>

// Function pointer type for handling commands with multiple arguments.
typedef char *(*CommandHandler)(char **args, int numArgs, bool isSlave);

// Struct to represent a command and its associated handler.
typedef struct {
  const char *command;
  CommandHandler handler;
} Command;

char *handleCommand(const char *command, char **args, int numArgs, bool isSlave);

char *handlePing(char **args, int numArgs, bool isSlave);
char *handleEcho(char **args, int numArgs, bool isSlave);
char *handleSet(char **args, int numArgs, bool isSlave);
char *handleGet(char **args, int numArgs, bool isSlave);
char* handleInfo(char **args, int numArgs, bool isSlave);
char* handleReplConf(char **args, int numArgs, bool isSlave);
char* handleReplConfCapaPsync2(char **args, int numArgs, bool isSlave);
char* handlePsync(char **args, int numArgs, bool isSlave);
char* handleAck(char **args, int numArgs, bool isSlave);
void updateOffsetForCommand(const char* command);

extern Command commandTable[];
extern KeyValueStore store;

#endif // COMMAND_HANDLER_H

