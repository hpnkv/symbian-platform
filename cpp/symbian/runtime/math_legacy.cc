// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "math_legacy.h"

extern "C" float ceilf(float value) {
  return symbian::runtime::math::Ceil(value);
}

extern "C" double frexp(double value, int* absl_nonnull exponent) {
  return symbian::runtime::math::Frexp(value, exponent);
}

extern "C" double ldexp(double value, int exponent) {
  return symbian::runtime::math::Ldexp(value, exponent);
}

extern "C" double modf(double value, double* absl_nonnull integral) {
  return symbian::runtime::math::Modf(value, integral);
}

extern "C" float nextafterf(float value, float toward) {
  return symbian::runtime::math::Nextafter(value, toward);
}

extern "C" double round(double value) {
  return symbian::runtime::math::Round(value);
}

extern "C" float scalbnf(float value, int exponent) {
  return symbian::runtime::math::Scalbn(value, exponent);
}
