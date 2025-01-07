#include "command-handler.h"
#include "streams.h"
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

extern Command commandTable[];
extern RDBConfig rdbConfig;
extern KeyValueStore store;
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
  uint64_t expiry = 0;

  if (isSlave) {
    offset += 25 + strlen(key) + strlen(value);
  } else {
    masterOffset += 25 + strlen(key) + strlen(value);
  }

  if (numArgs > 3) {
    expiry = currentTime() + atoi(args[3]);
    printf("Expiry in SET: %lld\n", expiry);

    if (isSlave) {
      offset += 14 + strlen(args[3]);
    } else {

      masterOffset += 14 + strlen(args[3]);
    }
  }

  // Check for expiry argument which is optional

  printf("Setting key %s to value %s with expiry %lld\n", key, value, expiry);
  setKeyValue(&store, key, value, expiry);
  return strdup("+OK\r\n");
}

char *handleGet(char **args, int numArgs, bool isSlave) {
  printf("Starting GET command handling\n");

  if (args[0] == NULL) {
    printf("Error: Key argument required\n");
    return strdup("-ERROR Key argument required\r\n");
  }

  printf("Fetching value for key: %s\n", args[0]);
  const char *value = getKeyValue(&store, args[0]);

  if (value == NULL) {
    printf("Key not found or expired\n");
    return strdup("$-1\r\n"); // RESP format for null bulk string
  }

  printf("Found value: %s\n", value);
  int len = strlen(value);
  printf("Value length: %d\n", len);

  char *response = malloc(len + 20); // Enough space for protocol overhead
  if (response == NULL) {
    printf("Error: Memory allocation failed\n");
    return strdup("-ERROR Memory allocation failed\r\n");
  }

  sprintf(response, "$%d\r\n%s\r\n", len, value);
  printf("Prepared response: %s\n", response);

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

int digitCount(size_t num) {
  int count = 1;
  while (num >= 10) {
    num /= 10;
    count++;
  }
  return count;
}
char *handleKeys(char **args, int numArgs, bool isSlave) {
  (void)args;    // Not used
  (void)numArgs; // Not used
  (void)isSlave; // Not used

  int size = lengthOfStore(&store); // Assuming store is a global variable

  if (size == 0) {
    return strdup("*0\r\n"); // No keys to return
  }

  // Calculate the total length of the response
  int totalLength = 0;
  char **keys = malloc(sizeof(char *) * size);
  if (!keys) {
    return strdup("-ERROR Memory allocation failed\r\n");
  }

  // Collect all keys and calculate the needed buffer size
  for (int i = 0; i < size; i++) {
    keys[i] = strcpy(malloc(MAX_KEY_LENGTH), getKeyAtIdx(&store, i));
    if (!keys[i]) {
      while (i-- > 0)
        free(keys[i]); // Cleanup on failure
      free(keys);
      return strdup("-ERROR Retrieving key failed\r\n");
    }
    // +3 for $, \r, \n; + int length of the key size; + strlen of key; +2 for
    // \r\n after key
    totalLength += strlen(keys[i]) + digitCount(strlen(keys[i])) + 6;
  }

  // Allocate response buffer
  int responseLength =
      digitCount(size) + 5 + totalLength; // +5 for "*<size>\r\n"
  char *response = malloc(responseLength);
  if (!response) {
    for (int i = 0; i < size; i++)
      free(keys[i]);
    free(keys);
    return strdup("-ERROR Memory allocation failed\r\n");
  }

  // Write the initial part of the response
  char *ptr = response;
  ptr += sprintf(ptr, "*%d\r\n", size);

  // Append all keys to the response
  for (int i = 0; i < size; i++) {
    ptr += sprintf(ptr, "$%lu\r\n%s\r\n", strlen(keys[i]), keys[i]);
  }

  free(keys); // Cleanup key array
  return response;
}

// Helper function to count digits in an integer

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

char *handleType(char **args, int numArgs, bool isSlave) {
  if (numArgs < 1) {
    return strdup("-ERROR Insufficient arguments\r\n");
  }

  const char *type = getType(&store, args[0]);

  if (isSlave) {
    offset += 14 + strlen(args[0]);
  }

  return strdup(type);
}

char *handleXadd(char **args, int numArg, bool isSlave) {
  if (numArg < 3) { // Minimum args should be key, id, and at least one field.
    return strdup("-ERROR Insufficient arguments\r\n");
  }

  const char *key = args[0];
  const char *id = args[1];
  const char **fields = (const char **)(args + 2);
  int numFields = numArg - 2; // Subtract key and id from total arguments.

  Stream *stream = findOrCreateStream(&store, strdup(key));
  if (!stream) {
    return strdup("-ERROR Failed to create stream\r\n");
  }

  Result res = xadd(&store, key, id, fields, numFields);
  if (!res.success) {
    return strdup(res.message);
  }

  if (isSlave) {
    // Increment replication offset based on the key, id, and field data.
    offset += 14 + strlen(key) + strlen(id);
    for (int i = 0; i < numFields; i++) {
      offset += 14 + strlen(fields[i]);
    }
  }

  // Retrieve the latest entry ID from the stream.
  int numEntries = stream->numEntries;
  char *entryId = stream->entries[numEntries - 1].id;

  // Calculate the length of the RESP string.
  size_t len = strlen(entryId);
  char *response = malloc(len + 20); // Allocate space for formatting.
  if (!response) {
    return strdup("-ERROR Memory allocation failed\r\n");
  }

  // Format the response as a RESP bulk string.
  sprintf(response, "$%zu\r\n%s\r\n", len, entryId);
  return response;
}

char *handleXrange(char **args, int numArg, bool isSlave) {

  if (numArg < 3) {
    return strdup("-ERROR Insufficient arguments\r\n");
  }

  const char *key = args[0];
  const char *startID = args[1];
  const char *endID = args[2];

  Result res = xrange(&store, key, startID, endID);

  if (isSlave) {
    offset += 14 + strlen(key) + strlen(startID) + strlen(endID);
  }

  return strdup(res.message);
}

char *handleXread(char **args, int numArg, bool isSlave) {
  if (numArg < 3) {
    return strdup("-ERR Insufficient arguments\r\n");
  }

  int blockTime = 0; // Default is no blocking
  int streamsIndex = 0;

  // Check for the "BLOCK" option
  if (strcmp(args[0], "block") == 0) {
    if (numArg < 5) {
      return strdup("-ERR Insufficient arguments for BLOCK\r\n");
    }

    blockTime = atoi(args[1]);
    streamsIndex = 2; // Skip "BLOCK" and block time
  }

  if (strcmp(args[streamsIndex], "streams") != 0) {
    return strdup("-ERR Expected 'streams' keyword\r\n");
  }

  int numStreams = (numArg - streamsIndex - 1) / 2;
  if (numArg != streamsIndex + 1 + 2 * numStreams) {
    return strdup("-ERR Insufficient arguments\r\n");
  }

  const char **keys = (const char **)malloc(sizeof(char *) * numStreams);
  const char **ids = (const char **)malloc(sizeof(char *) * numStreams);

  if (!keys || !ids) {
    free(keys);
    free(ids);
    return strdup("-ERR Memory allocation failed\r\n");
  }

  // Extract keys and IDs
  for (int i = 0; i < numStreams; i++) {
    keys[i] = args[streamsIndex + 1 + i];
    ids[i] = args[streamsIndex + 1 + numStreams + i];
  }

  char *response = xread(keys, ids, numStreams, isSlave, &store, blockTime);

  free(keys);
  free(ids);

  return response;
}

char *handleIncrement(char **args, int numArgs, bool isSlave) {
  if (numArgs < 1) {
    return strdup("-ERR Insufficient arguments for INCREMENT\r\n");
  }

  const char *key = args[0];
  if (key == NULL) {
    return strdup("-ERR Key is Null\r\n");
  }

  const void *stored_value = getKeyValue(&store, key);

  if (stored_value != NULL) {
    // Check if the stored value is a valid number
    char *endptr;
    const char *str_value = (const char *)stored_value;
    long long currentValue = strtoll(str_value, &endptr, 10);

    // If endptr points to anything other than '\0', the conversion failed
    if (*endptr != '\0') {
      return strdup("-ERR value is not an integer or out of range\r\n");
    }

    currentValue++;
    char valueStr[32];
    snprintf(valueStr, sizeof(valueStr), "%lld", currentValue);
    setKeyValue(&store, key, valueStr, 0);

    char *response = malloc(32);
    snprintf(response, 32, ":%lld\r\n", currentValue);
    return response;
  } else {
    // Key doesn't exist, start with 1
    setKeyValue(&store, key, "1", 0);
    return strdup(":1\r\n");
  }
}
char *handleMulti(char **args, int numArgs, bool isSlave) {
  (void)args;    // Unused parameter
  (void)numArgs; // Unused parameter
  (void)isSlave; // Unused parameter

  return strdup("+OK\r\n");
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
    {"WAIT", handleWait},
    {"CONFIG", handleRDBConfig},
    {"KEYS", handleKeys},
    {"TYPE", handleType},
    {"XADD", handleXadd},
    {"XRANGE", handleXrange},
    {"XREAD", handleXread},
    {"INCR", handleIncrement},
    {"MULTI", handleMulti},

    {NULL, NULL} // End of the command table
                 //
};
