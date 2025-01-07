#ifndef RESP_H
#define RESP_H

#include <stddef.h>

#define RESP_OK 0
#define RESP_ERR -1
#define RESP_INCOMPLETE 1

#define RESP_BUFFER_SIZE 4096

typedef enum {
  RespTypeString,
  RespTypeError,
  RespTypeInteger,
  RespTypeBulk,
  RespTypeArray
} RespType;

typedef struct RespValue {
  RespType type;
  union {
    struct {
      char *str;
      size_t len;
    } string;
    long long integer;
    struct {
      struct RespValue **elements;
      size_t len;
    } array;
  } data;
} RespValue;

typedef struct RespBuffer {
  char *buffer;
  size_t size;
  size_t used;
} RespBuffer;

RespBuffer *createRespBuffer(void);
void freeRespBuffer(RespBuffer *buffer);
int appendRespBuffer(RespBuffer *buffer, const char *data, size_t len);
int parseResp(RespBuffer *buffer, RespValue **value);
void freeRespValue(RespValue *value);
char *encodeBulkString(const char *str, size_t len, size_t *outputLen);

#endif
