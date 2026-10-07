// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

// The source-workspace older-runtime profile currently supports the C locale.
// These C ABI entry points use the guaranteed ASCII subset of that locale.
extern "C" int isspace(int character) {
  return character == ' ' || (character >= '\t' && character <= '\r');
}

extern "C" int tolower(int character) {
  return character >= 'A' && character <= 'Z' ? character + ('a' - 'A')
                                              : character;
}

extern "C" int toupper(int character) {
  return character >= 'a' && character <= 'z' ? character - ('a' - 'A')
                                              : character;
}
