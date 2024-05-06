#include "utils.h"
#include <arpa/inet.h>
#include <stdio.h>

void toUpper(char *str) {

  if (str == NULL) {
    return;
  }

  while (*str != '\0') {
    if (*str >= 'a' && *str <= 'z') {
      *str = *str - 'a' + 'A';
    }
    str++;
  }
}

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Function to find the index of a character in the base64 table
static inline int base64_index(char c) {
  const char *p = strchr(base64_chars, c);
  if (p) {
    return p - base64_chars;
  } else {
    return -1;
  }
}

// Decode a base64 encoded string to an array of bytes
char *base64_decode(const char *hex) {
  char *bytes = malloc(strlen(hex) / 2);
  int a, b, i = 0;
  while (2 * i + 1 < strlen(hex)) {
    a = hex[2 * i] - (hex[2 * i] >= 97 ? 87 : 48);
    b = hex[2 * i + 1] - (hex[2 * i + 1] >= 97 ? 87 : 48);
    bytes[i++] = a * 16 + b;
    if (!a && !b) {
      bytes[i - 1] = 32; /* strlen cheating */
    }
  }
  bytes[i] = '\0';
  return (bytes);
}

char *createRDBFile() {
  const char *hexEncoded =
      "524544495330303131fa0972656469732d76657205372e322e30fa0a7265"
      "6469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d"
      "656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";
  char *decodedData = base64_decode(hexEncoded);
  if (!decodedData) {
    return NULL;
  }

  size_t decodedLength = strlen(decodedData);
  char *rdb =
      malloc(decodedLength + 50); // Ensure to have enough space for the header
  if (!rdb) {
    free(decodedData);
    return NULL;
  }

  sprintf(rdb, "$%zu\r\n%s", decodedLength, decodedData);
  free(decodedData); // Free the decoded data after using it
  return rdb;
}

size_t digitsInNumber(size_t num) {
    size_t digits = 0;
    do {
        digits++;
        num /= 10;
    } while (num != 0);
    return digits;
}

