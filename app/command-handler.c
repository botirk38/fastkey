#include "command-handler.h"
#include <string.h>
#include <stdio.h>

char *handlePing(const char *args) {
    // RESP Simple String
    return strdup("+PONG\r\n");
}

char *handleEcho(const char *args) {
    if (args == NULL) {
        return strdup("-ERROR No argument provided\r\n");
    }
    int len = strlen(args);
    char *response = malloc(len + 20); 
    if (response == NULL) {
        return NULL; 
    }
    sprintf(response, "$%d\r\n%s\r\n", len, args);
    return response;
}

char *handleCommand(const char *command, const char *args) {
    for (int i = 0; commandTable[i].command != NULL; i++) {
        if (strcmp(command, commandTable[i].command) == 0) {
            char *response = commandTable[i].handler(args);
            if (response == NULL) {
                return strdup("-ERROR Handler failed\r\n");
            }
            return response;
        }
    }
    return strdup("-ERROR Unknown command\r\n");
}

// Definition of the command table.
Command commandTable[] = {
    {"PING", handlePing},
    {"ECHO", handleEcho},
    {NULL, NULL} // End of the command table
};

