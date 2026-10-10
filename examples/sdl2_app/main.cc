// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "absl/log/check.h"
#include "sdl2_app/application.h"

int main() {
  CHECK_OK(arkanoid::Run());
  return 0;
}
