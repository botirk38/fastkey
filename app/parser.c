#include "parser.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SELECTDB 0xFE
#define RESIZEDB 0xFB
#define AUX 0xFA
#define EXPIRETIME 0xFD
#define EXPIRETIMEMS 0xFC
#define EOF_OP 0xFF
#define KEYVALUE 0

RespCommand *parseCommand(char *buffer) {
  RespCommand *command = malloc(sizeof(RespCommand));
  if (!command) {
    return NULL;
  }
  command->command = NULL;
  command->args = NULL;
  command->numArgs = 0;

  int numElements = atoi(buffer + 1);
  command->args = malloc(sizeof(char *) * (numElements - 1));

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
    }

    else {
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

bool isWriteCommand(const char *command) {

  if (strcmp(command, "SET") == 0) {
    return true;
  }

  return false;
}

// Function to read length encoding
uint64_t readLengthEncoding(FILE *file) {
  unsigned char buf;
  fread(&buf, sizeof(buf), 1, file);

  switch (buf >> 6) {
  case 0:
    return buf & 0x3F;
  case 1: {
    uint64_t len = buf & 0x3F;
    fread(&buf, sizeof(buf), 1, file);
    return (len << 8) | buf;
  }
  case 2: {
    uint64_t len;
    fread(&len, sizeof(len), 1, file);
    return len;
  }
  default:
    return 0; // Special format not handled
  }
}

// Function to read a string encoded value
char *readString(FILE *file) {
  uint64_t length = readLengthEncoding(file);
  char *str = malloc(length + 1);
  fread(str, sizeof(char), length, file);
  str[length] = '\0';
  return str;
}

// Function to parse a key-value pair
void parse_key_value(FILE *rdb_file, KeyValueStore *store) {
  // Read value type
  unsigned char value_type;
  fread(&value_type, sizeof(unsigned char), 1, rdb_file);
  printf("Value Type: %02X\n", value_type);

  int expiry = readLengthEncoding(rdb_file);
  printf("Expiry: %d\n", expiry);
  // Parse the key as a string
  char *key = readString(rdb_file);

  // Parse the value based on the value type
  switch (value_type) {
  case 0x00: { // String encoding

    char *value = readString(rdb_file);

    printf("Key: %s, Value: %s \n", key, value);

    setKeyValue(store, key, value, 0);

    break;
  }

  default:
    printf("Unsupported value type\n");
    break;
  }
}

// Function to parse RDB file
void parseRDBFile(const char *filename, const char *dir, KeyValueStore *store) {
  char path[256];
  snprintf(path, sizeof(path), "%s/%s", dir, filename);
  FILE *file = fopen(path, "rb");
  if (!file) {
    printf("Error: Unable to open file %s\n", filename);
    return;
  }

  // Read the initial header
  char magic[6];
  fread(magic, sizeof(char), 5, file);
  magic[5] = '\0';
  printf("Magic: %s\n", magic);
  if (strcmp(magic, "REDIS") != 0) {
    printf("Error: Invalid RDB file format\n");
    fclose(file);
    return;
  }

  char version[6];
  fread(version, sizeof(char), 4, file);
  version[5] = '\0';
  printf("Version: %s\n", version);
  int rdbVersion = atoi(version);
  printf("RDB Version: %d\n", rdbVersion);

  // Loop until end of file
  unsigned char opcode;
  uint64_t expireTime = 0;
  while (fread(&opcode, sizeof(opcode), 1, file)) {

    printf("Opcode: %02X\n", opcode);

    switch (opcode) {
    case SELECTDB: {
      uint64_t dbNumber = readLengthEncoding(file);
      printf("Database Selector: %lu\n", dbNumber);

      break;
    }
    case RESIZEDB: {
      uint64_t hashTableSize = readLengthEncoding(file);
      uint64_t expireHashTableSize = readLengthEncoding(file);
      printf("Resizedb: Hash Table Size: %lu, Expire Hash Table Size: %lu\n",
             hashTableSize, expireHashTableSize);

      for (int i = 0; i < hashTableSize; i++) {
        parse_key_value(file, store);
      }

      break;
    }
    case AUX: {
      char *key = readString(file);
      char *value = readString(file);
      printf("Auxiliary Field: Key: %s, Value: %s\n", key, value);
      free(key);
      free(value);
      break;
    }

    case EOF_OP: {
      printf("End of RDB file\n");
      fclose(file);
      return;
    }

    default:
      continue;
    }
  }
  fclose(file);
}
