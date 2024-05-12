#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

long long currentTime();
void toUpper(char *str);
char* base64_decode(const char* hex);
char* createRDBFile();
size_t digitsInNumber(size_t number);


#endif // UTILS_H
