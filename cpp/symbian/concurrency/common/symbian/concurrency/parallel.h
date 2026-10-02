// Copyright 2026 The A11 Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
// http://www.apache.org/licenses/LICENSE-2.0
//
// Stackless guest adaptation of A11 cpp/a11/concurrency/parallel.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b. A11's AwaitAll waits for
// every input, even after an error, preserving input order. This form returns
// a Future so the event thread can observe the same result without blocking.

#ifndef SYMBIAN_CONCURRENCY_PARALLEL_H_
#define SYMBIAN_CONCURRENCY_PARALLEL_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "symbian/concurrency/future.h"

namespace symbian::concurrency {

template <typename T>
Future<std::vector<absl::StatusOr<T>>> JoinAll(std::vector<Future<T>> inputs) {
  struct Collector {
    explicit Collector(size_t count) : slots(count), remaining(count) {}

    thread::Mutex mu;
    std::vector<std::optional<absl::StatusOr<T>>> slots;
    size_t remaining;
    Promise<std::vector<absl::StatusOr<T>>> promise;
  };

  auto collector = std::make_shared<Collector>(inputs.size());
  auto joined = collector->promise.future();
  collector->promise.SetCancellationCallback([inputs] {
    for (const auto& input : inputs) {
      input.Cancel();
    }
  });
  if (inputs.empty()) {
    collector->promise.SetValue({});
    return joined;
  }
  for (size_t index = 0; index < inputs.size(); ++index) {
    inputs[index].OnReady([collector, index](const absl::StatusOr<T>& result) {
      bool complete = false;
      std::vector<absl::StatusOr<T>> results;
      {
        thread::MutexLock lock(&collector->mu);
        collector->slots[index] = result;
        complete = --collector->remaining == 0;
        if (complete) {
          results.reserve(collector->slots.size());
          for (auto& slot : collector->slots) {
            results.push_back(std::move(*slot));
          }
        }
      }
      if (complete) {
        collector->promise.SetValue(std::move(results));
      }
    });
  }
  return joined;
}

}  // namespace symbian::concurrency

#endif  // SYMBIAN_CONCURRENCY_PARALLEL_H_
