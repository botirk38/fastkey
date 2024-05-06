#include "command-handler.h"
#include "utils/KeyValueStore.h"
#include "utils/utils.h"
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

int replicasProcessed;
size_t offset = 0;
size_t masterOffset = 0;

// Global mutex and condition variable
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

char *handlePing(char **args, int numArgs, bool isSlave) {
  (void)args;    // Unused parameter
  (void)numArgs; // Unused parameter

  if (isSlave) {
    offset += 14;
  }

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

  if (isSlave) {
    offset += 25 + strlen(key) + strlen(value);
  } else {
    masterOffset += 25 + strlen(key) + strlen(value);
  }

  if (numArgs > 3) {
    expiry = atoi(args[3]);

    if (isSlave) {
      offset += 14 + strlen(args[3]);
    } else {

      masterOffset += 14 + strlen(args[3]);
    }
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

char *handleKeys(char **args, int numArgs, bool isSlave) {
  (void)numArgs;
  (void)isSlave;

  if (numArgs < 1) {
    return strdup("-ERROR Insufficient arguments\r\n");
  }

  int size = lengthOfStore(&store); // Assuming store is a global variable
  char *response = NULL;

  for (int i = 0; i < size; i++) {
    const char *key =
        getKeyAtIdx(&store, i); // Assuming getKeyAtIdx is correctly implemented
    printf("Key: %s\n", key);

    char *response =
        malloc(strlen(key) + 20); // Enough space for protocol overhead

    if (response == NULL) {
      return strdup("-ERROR Memory allocation failed\r\n");
    }

    sprintf(response, "*%d\r\n$%lu\r\n%s\r\n", size, strlen(key), key);
  }

  return response;
}

char *handleRDBConfig(char **args, int numArgs, bool isSlave) {
  if (numArgs < 1) {
    return strdup("-ERROR Insufficient arguments\r\n");
  }

  if (strcmp(args[0], "GET") == 0) {
    if (numArgs < 2) {
      return strdup("-ERROR Insufficient arguments\r\n");
    }

    if (strcmp(args[1], "dir") == 0) {
      char response[MAX_DIR_SIZE + 20] = {0};
      sprintf(response, "*2\r\n$3\r\ndir\r\n$%lu\r\n%s\r\n",
              strlen(rdbConfig.dir), rdbConfig.dir);
      return strdup(response);
    } else if (strcmp(args[1], "dbfilename") == 0) {
      char response[MAX_FILE_NAME_SIZE + 20] = {0};
      sprintf(response, "*2\r\n$10\r\ndbfilename\r\n$%lu\r\n%s\r\n",
              strlen(rdbConfig.dbFileName), rdbConfig.dbFileName);
      return strdup(response);
    }

    return strdup("-ERROR Invalid argument\r\n");
  }

  return strdup("-ERROR Invalid argument\r\n");
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

  } else if (numArgs == 2 && strcmp(args[0], "ACK") == 0 && !isSlave) {

    printf("Received ACK in Command Handler\n");

    int replicasOffset = atoi(args[1]);
    printf("Replicas Offset: %d\n", replicasOffset);
    printf("Offset: %zu\n", offset);
    printf("Master Offset: %zu\n", masterOffset);

    if (replicasOffset >= masterOffset) {

      ackReceived();
    }

    response = "+OK\r\n";

  }

  else {

    response = "+OK\r\n";
  }

  if (isSlave) {
    offset += 37;
  }

  printf("Response for REPLCONF: %s\n", response);
  return response;
}

int getCurrentTimeMillis() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
char *handleWait(char **args, int numArgs, bool isSlave) {
  if (numArgs < 3) {
    return strdup("-ERROR Insufficient arguments\r\n");
  }

  char *response = malloc(30);
  if (response == NULL) {
    return strdup("-ERROR Memory allocation failed\r\n");
  }

  int minimumReplicas = atoi(args[0]);
  int timeout = atoi(args[1]);
  int expectedReplicas = atoi(args[2]);
  replicasProcessed = 0; // Reset the count of processed replicas

  if (masterOffset == 0) {
    snprintf(response, 30, ":%d\r\n", expectedReplicas);
    return response;
  }

  pthread_mutex_lock(&mutex);
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += timeout / 1000;
  ts.tv_nsec += (timeout % 1000) * 1000000;

  // Wait until the number of replicasProcessed reaches the minimum required
  // or timeout occurs
  while (replicasProcessed < expectedReplicas) {
    int res = pthread_cond_timedwait(&cond, &mutex, &ts);
    if (res == ETIMEDOUT) {
      break;
    }
  }

  snprintf(response, 30, ":%d\r\n", replicasProcessed);

  pthread_mutex_unlock(&mutex);

  return response; // Return the number of replicas that have processed the
                   // command
}

void ackReceived() {
  pthread_mutex_lock(&mutex);
  replicasProcessed++;
  printf("Replicas processed Increased: %d\n", replicasProcessed);
  pthread_cond_signal(&cond); // Signal that an ack was received
  pthread_mutex_unlock(&mutex);
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
    {"PING", handlePing},   {"ECHO", handleEcho}, {"SET", handleSet},
    {"GET", handleGet},     {"INFO", handleInfo}, {"REPLCONF", handleReplConf},
    {"PSYNC", handlePsync}, {"WAIT", handleWait}, {"CONFIG", handleRDBConfig},
    {"KEYS", handleKeys},   {NULL, NULL} // End of the command table
                                         //
};
