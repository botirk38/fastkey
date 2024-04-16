#include "command-handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *handlePing(char **args, int numArgs, bool isSlave) {
  (void)args;    // Unused parameter
  (void)numArgs; // Unused parameter
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

  if (numArgs > 3) {
    expiry = atoi(args[3]);
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

  (void)numArgs;
  (void)args;
  (void)isSlave;

  char *response = "+OK\r\n";

  return response;
}

char* handlePsync(char **args, int numArgs, bool isSlave) {
  return strdup("+FULLRESYNC 8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb 0\r\n");
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
    {"PING", handlePing}, {"ECHO", handleEcho}, {"SET", handleSet},
    {"GET", handleGet},   {"INFO", handleInfo}, {"REPLCONF", handleReplConf}, {"PSYNC", handlePsync},
    {NULL, NULL} // End of the command table
    //
};
