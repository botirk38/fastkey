#include "command-handler.h"
#include "utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t offset = 0;

void updateOffsetForCommand(const char *command) {
  printf("Calculating offset \n");
  size_t commandLength = strlen(command);
  offset += commandLength; // Add the length of the command string to the offset
  printf("Offset %zu\n", offset);
}

char *handlePing(char **args, int numArgs, bool isSlave) {
  (void)args;    // Unused parameter
  (void)numArgs; // Unused parameter

  offset += 14;
  return strdup("+PONG\r\n");
}

char *handleEcho(char **args, int numArgs, bool isSlave) {
  (void)numArgs; // Unused parameter
  if (args == NULL || args[0] == NULL) {
    return strdup("-ERROR No argument provided\r\n");
  }
  int len = strlen(args[0]);
  char *response = malloc(len + 20); // Enough space for protocol overhead
  if (response == NULL) {
    return NULL;
  }
  sprintf(response, "$%d\r\n%s\r\n", len, args[0]);
  return response;
}

char *handleSet(char **args, int numArgs, bool isSlave) {
  if (args[0] == NULL || args[1] == NULL) {
    return strdup("-ERROR Insufficient arguments\r\n");
  }
  const char *key = args[0];
  const char *value = args[1];
  int expiry = 0;
  offset += 25 + strlen(key) + strlen(value);

  if (numArgs > 3) {
    expiry = atoi(args[3]);
    offset += 14 + strlen(args[3]);
  }

  // Check for expiry argument which is optional

  printf("Setting key %s to value %s with expiry %d\n", key, value, expiry);
  setKeyValue(&store, key, value, expiry);
  return strdup("+OK\r\n");
}

char *handleGet(char **args, int numArgs, bool isSlave) {

  (void)numArgs; // Unused parameter

  if (args[0] == NULL) {
    return strdup("-ERROR Key argument required\r\n");
  }
  const char *value = getKeyValue(&store, args[0]);
  if (strcmp(value, "Key not found") == 0 ||
      strcmp(value, "Key has expired") == 0) {
    return strdup("$-1\r\n"); // RESP format for null bulk string
  }
  int len = strlen(value);
  char *response = malloc(len + 20); // Enough space for protocol overhead

  if (response == NULL) {
    return strdup("-ERROR Memory allocation failed\r\n");
  }

  sprintf(response, "$%d\r\n%s\r\n", len, value);
  return response;
}

char *handleInfo(char **args, int numArgs, bool isSlave) {
  (void)numArgs;
  (void)args;

  char *role = isSlave ? "slave" : "master";
  char *master_repl_id = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
  int master_repl_offset = 0;

  char info[256];

  sprintf(info, "role:%s\r\nmaster_replid:%s\r\nmaster_repl_offset:%d", role,
          master_repl_id, master_repl_offset);

  static char response[256];

  sprintf(response, "$%lu\r\n%s\r\n", strlen(info), info);

  return response;
}

char *handleReplConf(char **args, int numArgs, bool isSlave) {

  char *response;

  if (numArgs == 2 && strcmp(args[0], "GETACK") == 0 && isSlave) {
    printf("Received ACK in Command Handler\n");

    char responseBuffer[1024]; // Buffer to hold the response string
    sprintf(responseBuffer,
            "*3\r\n$8\r\nREPLCONF\r\n$3\r\nACK\r\n$%zu\r\n%zu\r\n",
            digitsInNumber(offset), offset);

    response = strdup(responseBuffer);

  } else {

    response = "+OK\r\n";
  }
  offset += 37;

  printf("Response for REPLCONF: %s\n", response);
  return response;
}

char *handleWait(char **args, int numArgs, bool isSlave) {

  printf("Num Replicas %s\n", args[2]);

  char *response = malloc(30);

  if (response == NULL) {
    return strdup("-ERROR Memory allocation failed\r\n");
  }

  snprintf(response, 30, ":%s\r\n", args[2]);
  return response;
}

char *handlePsync(char **args, int numArgs, bool isSlave) {
  char *response = "+FULLRESYNC 8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb 0\r\n";
  char *rdbFile = createRDBFile();

  if (rdbFile == NULL) {
    return strdup("-ERROR RDB file creation failed\r\n");
  }

  char *fullResponse = malloc(strlen(response) + strlen(rdbFile) + 1);

  if (fullResponse == NULL) {
    return strdup("-ERROR Memory allocation failed\r\n");
  }

  strcpy(fullResponse, response);
  strcat(fullResponse, rdbFile);

  free(rdbFile);

  return fullResponse;
}

char *handleAck(char **args, int numArgs, bool isSlave) {
  (void)args;
  (void)numArgs;
  (void)isSlave;

  return "+OK\r\n";
}

char *handleCommand(const char *command, char **args, int numArg,
                    bool isSlave) {
  for (int i = 0; commandTable[i].command != NULL; i++) {
    if (strcmp(command, commandTable[i].command) == 0) {
      printf("Handling command %s\n", command);
      return commandTable[i].handler(args, numArg, isSlave);
    }
  }
  return strdup("-ERROR Unknown command\r\n");
}

// Command table implementation
Command commandTable[] = {
    {"PING", handlePing},
    {"ECHO", handleEcho},
    {"SET", handleSet},
    {"GET", handleGet},
    {"INFO", handleInfo},
    {"REPLCONF", handleReplConf},
    {"PSYNC", handlePsync},
    {"ACK", handleAck},
    {"WAIT", handleWait},
    {NULL, NULL} // End of the command table
                 //
};
