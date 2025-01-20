#ifndef RDB_READER_H
#define RDB_READER_H

#include <arpa/inet.h>
#include <endian.h>
#include <stdint.h>
#include <stdio.h>

// File Format Constants
#define RDB_MAGIC "REDIS"
#define RDB_VERSION "0011"
#define RDB_MAGIC_LEN 5
#define RDB_VERSION_LEN 4

// Encoding Type Bits
#define RDB_TYPE_MASK 0xC0
#define RDB_TYPE_SHIFT 6
#define RDB_LENGTH_MASK 0x3F

// Length Encoding Types
#define RDB_LEN_6BIT 0    // 00: 6 bit len
#define RDB_LEN_14BIT 1   // 01: 14 bit len
#define RDB_LEN_32BIT 2   // 10: 32 bit len
#define RDB_LEN_SPECIAL 3 // 11: special encoding

// Special Format Markers
#define RDB_METADATA_START 0xFA
#define RDB_DATABASE_START 0xFE
#define RDB_HASHTABLE_SIZE 0xFB
#define RDB_EXPIRE_SEC 0xFD
#define RDB_EXPIRE_MS 0xFC
#define RDB_EOF 0xFF

// Value Types
#define RDB_TYPE_STRING 0x00
#define RDB_TYPE_LIST 0x01
#define RDB_TYPE_SET 0x02
#define RDB_TYPE_HASH 0x03
#define RDB_TYPE_STREAM 0x04

// String Encodings
#define RDB_ENC_INT8 0xC0
#define RDB_ENC_INT16 0xC1
#define RDB_ENC_INT32 0xC2
#define RDB_ENC_LZF 0xC3

typedef struct RdbReader {
  FILE *fp;
  char *filename;
  size_t pos;
} RdbReader;

RdbReader *createRdbReader(const char *dir, const char *filename);
void freeRdbReader(RdbReader *reader);
uint8_t readByte(RdbReader *reader);
uint32_t readUint32(RdbReader *reader);
uint64_t readUint64(RdbReader *reader);
size_t readLength(RdbReader *reader);
char *readString(RdbReader *reader, size_t *length);
char *readEncodedString(RdbReader *reader, size_t *length);
int validateHeader(RdbReader *reader);

#endif
