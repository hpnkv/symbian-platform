// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <absl/base/nullability.h>
#include <pthread.h>

namespace {

std::atomic<int> destroyed{0};

void DestroyKey(void* absl_nullable) {
  destroyed.fetch_add(1, std::memory_order_relaxed);
}

std::atomic<int> initialized{0};

void InitializeOnce() {
  initialized.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

int main() {
  pthread_key_t key = 0;
  pthread_once_t once = PTHREAD_ONCE_INIT;
  if (pthread_key_create(&key, DestroyKey) != 0) {
    return -601;
  }
  std::mutex mutex;
  std::condition_variable condition;
  int phase = 0;
  std::atomic<int> worker_result{0};
  std::thread worker([&] {
    if (pthread_once(&once, InitializeOnce) != 0 ||
        pthread_setspecific(key, &phase) != 0 ||
        pthread_getspecific(key) != &phase) {
      worker_result.store(-602);
    }
    {
      std::lock_guard<std::mutex> lock(mutex);
      phase = 1;
    }
    condition.notify_one();
    std::unique_lock<std::mutex> lock(mutex);
    if (!condition.wait_for(lock, std::chrono::seconds(2),
                            [&] { return phase == 2; })) {
      worker_result.store(-614);
    }
  });
  bool first_ready = false;
  {
    std::unique_lock<std::mutex> lock(mutex);
    first_ready = condition.wait_for(lock, std::chrono::seconds(2),
                                     [&] { return phase == 1; });
    if (pthread_once(&once, InitializeOnce) != 0) {
      return -603;
    }
    phase = 2;
  }
  condition.notify_one();
  worker.join();
  if (!first_ready) {
    return -615;
  }
  if (worker_result.load() != 0 || initialized.load() != 1 ||
      destroyed.load() != 1) {
    return -604;
  }
  std::unique_lock<std::mutex> lock(mutex);
  if (condition.wait_for(lock, std::chrono::milliseconds(4)) !=
      std::cv_status::timeout) {
    return -605;
  }
  lock.unlock();

  pthread_mutex_t static_mutex = PTHREAD_MUTEX_INITIALIZER;
  if (pthread_mutex_lock(&static_mutex) != 0) {
    return -606;
  }
  std::atomic<int> competing_trylock{0};
  std::thread competitor([&] {
    competing_trylock.store(pthread_mutex_trylock(&static_mutex),
                            std::memory_order_release);
  });
  competitor.join();
  if (competing_trylock.load(std::memory_order_acquire) != EBUSY) {
    return -619;
  }
  if (pthread_mutex_trylock(&static_mutex) != EBUSY) {
    return -610;
  }
  if (pthread_mutex_unlock(&static_mutex) != 0) {
    return -611;
  }
  if (pthread_mutex_trylock(&static_mutex) != 0) {
    return -616;
  }
  if (pthread_mutex_unlock(&static_mutex) != 0) {
    return -617;
  }
  if (pthread_mutex_destroy(&static_mutex) != 0) {
    return -612;
  }
  pthread_mutex_t recursive_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  if (pthread_mutex_lock(&recursive_mutex) != 0 ||
      pthread_mutex_trylock(&recursive_mutex) != 0 ||
      pthread_mutex_unlock(&recursive_mutex) != 0 ||
      pthread_mutex_unlock(&recursive_mutex) != 0 ||
      pthread_mutex_destroy(&recursive_mutex) != 0) {
    return -607;
  }
  if (pthread_key_delete(key) != 0 || pthread_getspecific(key) != nullptr ||
      pthread_setspecific(key, &phase) != EINVAL) {
    return -608;
  }

  std::atomic<int> detached_done{0};
  std::thread detached(
      [&] { detached_done.store(1, std::memory_order_release); });
  detached.detach();
  for (int attempt = 0;
       attempt < 1000 && detached_done.load(std::memory_order_acquire) == 0;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (detached_done.load(std::memory_order_acquire) != 1) {
    return -609;
  }
  std::mutex contended_mutex;
  int increments = 0;
  auto increment = [&] {
    for (int iteration = 0; iteration < 200; ++iteration) {
      std::lock_guard<std::mutex> guard(contended_mutex);
      ++increments;
    }
  };
  std::thread first(increment);
  std::thread second(increment);
  increment();
  first.join();
  second.join();
  if (increments != 600) {
    return -613;
  }
  std::mutex broadcast_mutex;
  std::condition_variable broadcast_condition;
  std::atomic<int> waiting{0};
  bool released = false;
  std::atomic<int> awakened{0};
  auto wait_for_broadcast = [&] {
    std::unique_lock<std::mutex> guard(broadcast_mutex);
    waiting.fetch_add(1, std::memory_order_release);
    if (broadcast_condition.wait_for(guard, std::chrono::seconds(2),
                                     [&] { return released; })) {
      awakened.fetch_add(1, std::memory_order_relaxed);
    }
  };
  std::thread first_waiter(wait_for_broadcast);
  std::thread second_waiter(wait_for_broadcast);
  for (int attempt = 0;
       attempt < 1000 && waiting.load(std::memory_order_acquire) != 2;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  {
    std::lock_guard<std::mutex> guard(broadcast_mutex);
    released = true;
  }
  broadcast_condition.notify_all();
  first_waiter.join();
  second_waiter.join();
  if (awakened.load(std::memory_order_relaxed) != 2) {
    return -618;
  }
  return 0;
}
