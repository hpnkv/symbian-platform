// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_TIME_SLEEP_H_
#define SYMBIAN_API_TIME_SLEEP_H_

#include <chrono>

namespace symbian::api::time {

// Sleep on a thread-relative native timer. A timer creation failure returns
// immediately so frame pacing cannot hold the application indefinitely.
void SleepFor(std::chrono::nanoseconds duration);

}  // namespace symbian::api::time

#endif  // SYMBIAN_API_TIME_SLEEP_H_
