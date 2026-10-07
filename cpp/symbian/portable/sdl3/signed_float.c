/* Copyright 2026 The Symbian SDK Authors.
 * Licensed under the Apache License, Version 2.0.
 *
 * The installed 0.2.0 runtime predates the signed 64-bit float helper.
 * Keep this compatibility definition in the static SDL3 archive until an SDK
 * runtime carrying compiler-rt floatdisf.c is available to every consumer.
 */

extern float __aeabi_ul2f(unsigned long long value);

float __aeabi_l2f(long long value) {
  if (value >= 0) {
    return __aeabi_ul2f((unsigned long long)value);
  }
  unsigned long long magnitude = (unsigned long long)(-(value + 1)) + 1;
  return -__aeabi_ul2f(magnitude);
}
