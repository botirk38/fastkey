#include "utils.h"

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

char *base64_decode(const char *input) {
  if (!input)
    return NULL;

  BIO *b64, *bio;
  int decodeLen = strlen(input);
  char *buffer = (char *)malloc(decodeLen);
  if (!buffer)
    return NULL;

  // Setup base64 BIO Filter
  b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64,
                BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer

  // Setup memory buffer source
  bio = BIO_new_mem_buf(
      input, -1); // -1 for read-only buffer to infer length automatically
  bio = BIO_push(b64, bio);

  // Decode
  int decoded_length = BIO_read(bio, buffer, decodeLen);
  if (decoded_length <= 0) {
    free(buffer);
    buffer = NULL;
  } else {
    // Reallocate buffer to the exact decoded size
    buffer = (char *)realloc(buffer, decoded_length + 1);
    buffer[decoded_length] = '\0'; // Null-terminate the output string
  }

  // Cleanup
  BIO_free_all(bio); // bio includes b64, no need for separate BIO_free(b64)

  return buffer;
}
