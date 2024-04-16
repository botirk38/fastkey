#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>


void toUpper(char *str);
char* base64_decode(const char*input);



#endif // UTILS_H
