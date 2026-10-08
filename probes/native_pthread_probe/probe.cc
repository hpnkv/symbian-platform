// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <mutex>
#include <thread>

#include <absl/base/nullability.h>
#include <pthread.h>
#include <string.h>
#include <wchar.h>

namespace {

std::atomic<int> destroyed{0};

void DestroyKey(void* absl_nullable) {
  destroyed.fetch_add(1, std::memory_order_relaxed);
}

std::atomic<int> initialized{0};

void InitializeOnce() {
  initialized.fetch_add(1, std::memory_order_relaxed);
}

std::atomic<int> local_static_constructions{0};

struct LocalStatic {
  LocalStatic() { local_static_constructions.fetch_add(1); }
  int value = 47;
};

int ReadLocalStatic() {
  static LocalStatic value;
  return value.value;
}

}  // namespace

int main() {
#ifdef SYMBIAN_PROBE_LOCAL_STATIC
  std::atomic<int> ready{0};
  std::atomic<bool> start{false};
  std::atomic<int> local_result{0};
  auto check_local = [&] {
    ready.fetch_add(1);
    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }
    if (ReadLocalStatic() != 47) {
      local_result.store(-660);
    }
  };
  std::thread first_local_thread(check_local);
  std::thread second_local_thread(check_local);
  while (ready.load(std::memory_order_acquire) != 2) {
    std::this_thread::yield();
  }
  start.store(true, std::memory_order_release);
  first_local_thread.join();
  second_local_thread.join();
  if (local_result.load() != 0 || local_static_constructions.load() != 1 ||
      ReadLocalStatic() != 47) {
    return -661;
  }
#endif
#ifdef SYMBIAN_PROBE_LOCAL_C_ALLOC
  if (std::getenv("TZ") != nullptr) {
    return -628;
  }
  void* absl_nullable tiny = std::malloc(1);
  if (tiny == nullptr || reinterpret_cast<std::uintptr_t>(tiny) % 8 != 0) {
    return -630;
  }
  tiny = std::realloc(tiny, 1);
  if (tiny == nullptr || reinterpret_cast<std::uintptr_t>(tiny) % 8 != 0) {
    return -631;
  }
  std::free(tiny);
  char error_text[32] = {};
  if (::strerror_r(EINVAL, error_text, sizeof(error_text)) != 0 ||
      error_text[0] != 'I' || error_text[1] != 'n') {
    return -629;
  }
  auto* absl_nullable bytes = static_cast<std::uint8_t*>(std::malloc(16));
  if (bytes == nullptr || reinterpret_cast<std::uintptr_t>(bytes) % 8 != 0) {
    return -620;
  }
  for (int index = 0; index < 16; ++index) {
    bytes[index] = static_cast<std::uint8_t>(index + 1);
  }
  auto* absl_nullable grown =
      static_cast<std::uint8_t*>(std::realloc(bytes, 48));
  if (grown == nullptr) {
    std::free(bytes);
    return -621;
  }
  for (int index = 0; index < 16; ++index) {
    if (grown[index] != index + 1) {
      return -622;
    }
  }
  auto* absl_nullable shrunk =
      static_cast<std::uint8_t*>(std::realloc(grown, 8));
  if (shrunk == nullptr) {
    std::free(grown);
    return -623;
  }
  for (int index = 0; index < 8; ++index) {
    if (shrunk[index] != index + 1) {
      return -624;
    }
  }
  if (std::realloc(shrunk, std::numeric_limits<std::size_t>::max()) !=
          nullptr ||
      shrunk[0] != 1) {
    return -632;
  }
  std::free(shrunk);
  auto* absl_nullable zeroed = static_cast<std::uint8_t*>(std::calloc(16, 3));
  if (zeroed == nullptr) {
    return -625;
  }
  for (int index = 0; index < 48; ++index) {
    if (zeroed[index] != 0) {
      return -626;
    }
  }
  std::thread cross_thread_free([zeroed] { std::free(zeroed); });
  cross_thread_free.join();
  if (std::calloc(std::numeric_limits<std::size_t>::max(), 2) != nullptr) {
    return -627;
  }
#endif
#ifdef SYMBIAN_PROBE_LEGACY_C
  const timespec sleep_request{0, 5000000};
  if (::nanosleep(&sleep_request, nullptr) != 0) {
    return -656;
  }
  char formatted[32] = {};
  if (std::snprintf(formatted, sizeof(formatted), "%04d %.1f", 7, 1.5) !=
          8 ||
      std::strcmp(formatted, "0007 1.5") != 0) {
    return -650;
  }
  char* absl_nullable allocated = nullptr;
  if (::asprintf(&allocated, "ok %d", 9) != 4 || allocated == nullptr ||
      std::strcmp(allocated, "ok 9") != 0) {
    std::free(allocated);
    return -651;
  }
  std::free(allocated);
  char* absl_nullable parsed_end = nullptr;
  if (std::strtod(" -12.5e2tail", &parsed_end) != -1250.0 ||
      parsed_end == nullptr || std::strcmp(parsed_end, "tail") != 0) {
    return -652;
  }
  if (std::strtof("0x1.8p+1", &parsed_end) != 3.0f ||
      parsed_end == nullptr || *parsed_end != '\0') {
    return -653;
  }
  const std::time_t epoch = 0;
  std::tm utc = {};
  if (::gmtime_r(&epoch, &utc) == nullptr || utc.tm_year != 70 ||
      utc.tm_mon != 0 || utc.tm_mday != 1 || utc.tm_gmtoff != 0 ||
      utc.tm_zone == nullptr) {
    return -654;
  }
  char date[20] = {};
  if (::strftime(date, sizeof(date), "%Y-%m-%d", &utc) != 10 ||
      std::strcmp(date, "1970-01-01") != 0) {
    return -655;
  }
#endif
#ifdef SYMBIAN_PROBE_LOCAL_C_UTF8
  mbstate_t decode_state = {};
  wchar_t wide = 0;
  if (mbrtowc(&wide, "\xE2", 1, &decode_state) !=
          static_cast<std::size_t>(-2) ||
      mbrtowc(&wide, "\x82\xAC", 2, &decode_state) != 2 || wide != 0x20AC ||
      !mbsinit(&decode_state)) {
    return -640;
  }
  if (mbrtowc(&wide, "\xC0\xAF", 2, &decode_state) !=
          static_cast<std::size_t>(-1) ||
      errno != EILSEQ || !mbsinit(&decode_state)) {
    return -641;
  }
  char encoded[8] = {};
  mbstate_t encode_state = {};
  if (wcrtomb(encoded, static_cast<wchar_t>(0x20AC), &encode_state) != 3 ||
      std::memcmp(encoded, "\xE2\x82\xAC", 3) != 0) {
    return -642;
  }
  const char* absl_nullable narrow_source = "A\xE2\x82\xAC";
  wchar_t wide_text[4] = {};
  mbstate_t sequence_state = {};
  if (mbsnrtowcs(wide_text, &narrow_source, 5, 4, &sequence_state) != 2 ||
      narrow_source != nullptr || wide_text[0] != L'A' ||
      wide_text[1] != 0x20AC) {
    return -643;
  }
  const wchar_t wide_source[] = {L'A', static_cast<wchar_t>(0x20AC), 0};
  const wchar_t* absl_nullable wide_cursor = wide_source;
  mbstate_t output_state = {};
  if (wcsnrtombs(encoded, &wide_cursor, 3, sizeof(encoded), &output_state) !=
          4 ||
      wide_cursor != nullptr || std::memcmp(encoded, "A\xE2\x82\xAC", 4) != 0) {
    return -644;
  }
  std::thread partial_worker([] { mbrtowc(nullptr, "\xE2", 1, nullptr); });
  partial_worker.join();
  if (mbrtowc(&wide, "Q", 1, nullptr) != 1 || wide != L'Q') {
    return -645;
  }
#endif
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
