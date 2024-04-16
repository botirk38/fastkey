#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RespCommand *parseCommand(char *buffer) {
    RespCommand *command = malloc(sizeof(RespCommand));
    if (!command) {
        return NULL;
    }
    command->command = NULL;
    command->args = NULL;
    command->numArgs = 0;

    int numElements = atoi(buffer + 1);
    command->args = malloc(sizeof(char*) * (numElements - 1));

    buffer = strchr(buffer, '\n') + 1;
    if (!buffer) {
        freeRespCommand(command);
        return NULL;
    }

    for (int i = 0; i < numElements; i++) {
        if (*buffer != '$') {
            freeRespCommand(command);
            return NULL;
        }

        int length = atoi(buffer + 1);
        buffer = strchr(buffer, '\n') + 1;
        if (!buffer || length <= 0) {
            freeRespCommand(command);
            return NULL;
        }

        char *part = strndup(buffer, length);
        if (!part) {
            freeRespCommand(command);
            return NULL;
        }

        if (i == 0) {
            toUpper(part);
            command->command = part;
        } else {
            command->args[command->numArgs++] = part;
        }

        buffer += length + 2; // Move past the current part and CRLF
    }

    return command;
}

void freeRespCommand(RespCommand *cmd) {
    if (cmd) {
        free(cmd->command);
        for (int i = 0; i < cmd->numArgs; i++) {
            free(cmd->args[i]);
        }
        free(cmd->args);
        free(cmd);
    }
}

