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
