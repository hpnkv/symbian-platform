#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

int RunFeature(void* absl_nullable) {
  char buffer[32];
  const int written =
      snprintf(buffer, sizeof(buffer), "%lu", strtoul("42", nullptr, 10));
  return written == 2 && strcmp(buffer, "42") == 0 ? 0 : 1;
}

int main() {
  return classic_demo_ui::Show(_L("OPEN C LIBC"), _L("Formats the number 42."),
                               _L("Compares the C string."),
                               _L("FORMATTED TEXT MATCHED"), &RunFeature,
                               nullptr);
}
