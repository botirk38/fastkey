#include "rdb.h"
#include "redis_store.h"
#include <stdlib.h>
#include <string.h>

RdbReader *createRdbReader(const char *dir, const char *filename) {
  size_t pathLen = strlen(dir) + 1 + strlen(filename);
  char *fullPath = malloc(pathLen + 1);
  snprintf(fullPath, pathLen + 1, "%s/%s", dir, filename);

  FILE *fp = fopen(fullPath, "rb");
  if (!fp) {
    free(fullPath);
    return NULL;
  }

  RdbReader *reader = malloc(sizeof(RdbReader));
  reader->fp = fp;
  reader->filename = fullPath;
  reader->pos = 0;

  return reader;
}

void freeRdbReader(RdbReader *reader) {
  if (reader) {
    if (reader->fp)
      fclose(reader->fp);
    free(reader->filename);
    free(reader);
  }
}

uint8_t readByte(RdbReader *reader) {
  uint8_t byte;
  if (fread(&byte, 1, 1, reader->fp) != 1)
    return 0;
  reader->pos++;
  return byte;
}

uint32_t readUint32(RdbReader *reader) {
  uint32_t value;
  if (fread(&value, sizeof(value), 1, reader->fp) != 1)
    return 0;
  reader->pos += sizeof(value);
  return ntohl(value);
}

uint64_t readUint64(RdbReader *reader) {
  uint64_t value;
  if (fread(&value, sizeof(value), 1, reader->fp) != 1)
    return 0;
  reader->pos += sizeof(value);
  return value;
}

size_t readLength(RdbReader *reader) {
  uint8_t byte = readByte(reader);
  uint8_t type = (byte & RDB_TYPE_MASK) >> RDB_TYPE_SHIFT;

  switch (type) {
  case RDB_LEN_6BIT:
    return byte & RDB_LENGTH_MASK;
  case RDB_LEN_14BIT:
    return ((byte & RDB_LENGTH_MASK) << 8) | readByte(reader);
  case RDB_LEN_32BIT:
    return readUint32(reader);
  default:
    return 0;
  }
}

char *readString(RdbReader *reader, size_t *length) {
  *length = readLength(reader);
  if (*length == 0)
    return NULL;

  char *str = malloc(*length + 1);
  if (fread(str, 1, *length, reader->fp) != *length) {
    free(str);
    return NULL;
  }
  reader->pos += *length;
  str[*length] = '\0';
  return str;
}

char *readEncodedString(RdbReader *reader, size_t *length) {
  uint8_t byte = readByte(reader);
  if ((byte & 0xC0) >> 6 == 3) {
    char numStr[32];
    int numLen;

    switch (byte) {
    case RDB_ENC_INT8: {
      numLen = snprintf(numStr, sizeof(numStr), "%d", readByte(reader));
      break;
    }
    case RDB_ENC_INT16: {
      uint16_t val;
      fread(&val, sizeof(val), 1, reader->fp);
      reader->pos += sizeof(val);
      numLen = snprintf(numStr, sizeof(numStr), "%d", ntohs(val));
      break;
    }
    case RDB_ENC_INT32: {
      uint32_t val;
      fread(&val, sizeof(val), 1, reader->fp);
      reader->pos += sizeof(val);
      numLen = snprintf(numStr, sizeof(numStr), "%d", ntohl(val));
      break;
    }
    default:
      return NULL;
    }

    *length = numLen;
    char *str = malloc(numLen + 1);
    memcpy(str, numStr, numLen + 1);
    return str;
  }

  return readString(reader, length);
}

int validateHeader(RdbReader *reader) {
  char magic[RDB_MAGIC_LEN + 1];
  if (fread(magic, 1, RDB_MAGIC_LEN, reader->fp) != RDB_MAGIC_LEN)
    return 0;
  magic[RDB_MAGIC_LEN] = '\0';
  reader->pos += RDB_MAGIC_LEN;

  if (strcmp(magic, RDB_MAGIC) != 0)
    return 0;

  char version[RDB_VERSION_LEN + 1];
  if (fread(version, 1, RDB_VERSION_LEN, reader->fp) != RDB_VERSION_LEN)
    return 0;
  version[RDB_VERSION_LEN] = '\0';
  reader->pos += RDB_VERSION_LEN;

  return strcmp(version, RDB_VERSION) == 0;
}

int findDatabaseSection(RdbReader *reader) {
  uint8_t type;
  while ((type = readByte(reader)) != RDB_EOF) {
    if (type == RDB_DATABASE_START) {
      readLength(reader); // Skip database number
      if (readByte(reader) == RDB_HASHTABLE_SIZE) {
        readLength(reader); // Skip hash table sizes
        readLength(reader);
      }
      return 1;
    }
  }
  return 0;
}

void skipRdbValue(RdbReader *reader) {
  size_t skipLen;
  free(readString(reader, &skipLen));
}

char *getRdbValue(RdbReader *reader, const char *targetKey, size_t *valueLen) {
  printf("[RDB] Starting to read RDB file for key: %s\n", targetKey);

  if (!validateHeader(reader) || !findDatabaseSection(reader)) {
    return NULL;
  }

  printf("[RDB] Successfully found database section\n");

  uint8_t type;
  while ((type = readByte(reader)) != RDB_EOF) {
    printf("[RDB] Reading entry type: 0x%02x\n", type);

    // Handle expiry
    uint64_t expiry = 0;
    if (type == RDB_EXPIRE_MS) {
      expiry = readUint64(reader);
      printf("[RDB] Found millisecond expiry: %llu\n", expiry);
      type = readByte(reader);
    } else if (type == RDB_EXPIRE_SEC) {
      expiry = readUint32(reader) * 1000;
      printf("[RDB] Found second expiry: %llu\n", expiry);
      type = readByte(reader);
    }

    // Read key
    size_t keyLen;
    char *keyStr = readString(reader, &keyLen);
    if (!keyStr)
      break;

    printf("[RDB] Read key: %s (length: %zu)\n", keyStr, keyLen);
    int keyMatch =
        (strlen(targetKey) == keyLen && memcmp(keyStr, targetKey, keyLen) == 0);
    printf("[RDB] Key match: %s\n", keyMatch ? "yes" : "no");
    free(keyStr);

    // Handle value based on type
    if (type == RDB_TYPE_STRING) {
      printf("[RDB] Reading string value\n");
      char *value = readString(reader, valueLen);

      if (keyMatch) {
        time_t now = getCurrentTimeMs();
        printf("[RDB] Expiry %llu\n", expiry);
        printf("[RDB] Now %lu\n", now);
        if (expiry > 0 && now >= expiry) {
          printf("[RDB] Key expired at %llu, current time %lu\n", expiry, now);
          free(value);
          return NULL;
        }
        return value;
      }
      free(value);
    }
  }

  return NULL;
}

RespValue **getRdbKeys(RdbReader *reader, size_t *keyCount) {
  *keyCount = 0;

  if (!validateHeader(reader) || !findDatabaseSection(reader)) {
    return NULL;
  }

  // Initialize keys array
  size_t capacity = 16;
  RespValue **keys = malloc(capacity * sizeof(RespValue *));

  uint8_t type;
  while ((type = readByte(reader)) != RDB_EOF) {
    // Handle expiry
    if (type == RDB_EXPIRE_MS) {
      readUint64(reader);
      type = readByte(reader);
    } else if (type == RDB_EXPIRE_SEC) {
      readUint32(reader);
      type = readByte(reader);
    }

    // Read key
    size_t keyLen;
    char *keyStr = readString(reader, &keyLen);
    if (!keyStr)
      break;

    // Skip value
    if (type == RDB_TYPE_STRING) {
      skipRdbValue(reader);
    }

    // Grow array if needed
    if (*keyCount == capacity) {
      capacity *= 2;
      keys = realloc(keys, capacity * sizeof(RespValue *));
    }

    // Add key to array
    keys[*keyCount] = createRespString(keyStr, keyLen);
    (*keyCount)++;
    free(keyStr);
  }

  return keys;
}
