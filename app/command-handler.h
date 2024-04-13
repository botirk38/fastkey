#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function pointer type for handling commands.
typedef char *(*CommandHandler)(const char *args);

// Struct to represent a command and its associated handler.
typedef struct {
  const char *command;
  CommandHandler handler;
} Command;

char* handleCommand(const char *command, const char *args);

char *handlePing(const char *args);
char *handleEcho(const char *args);

extern Command commandTable[];

#endif // COMMAND_HANDLER_H

