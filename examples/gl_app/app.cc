#include "absl/log/check.h"
#include "application.h"

int main() {
  CHECK_OK(gl_app::RunApplication());
  return 0;
}
