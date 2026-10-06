#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
  char buffer[32];
  const int written =
      snprintf(buffer, sizeof(buffer), "%lu", strtoul("42", nullptr, 10));
  return written == 2 && strcmp(buffer, "42") == 0 ? 0 : 1;
}
