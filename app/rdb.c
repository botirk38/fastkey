#include "rdb.h"
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
  return be64toh(value);
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
