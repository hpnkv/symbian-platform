#include <atomic>
#include <thread>

#include <pthread.h>

namespace {

std::atomic<int> reclaimed{0};

void Reclaim(void* pointer) {
  if (pointer != nullptr) {
    reclaimed.fetch_add(1, std::memory_order_relaxed);
  }
}

}  // namespace

extern "C" int SymbianRuntimeTlsProbe() {
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  if (pthread_mutex_init(&mutex, nullptr) != 0) {
    return -277;
  }
  if (pthread_cond_init(&condition, nullptr) != 0) {
    pthread_mutex_destroy(&mutex);
    return -278;
  }
  auto close_sync = [&] {
    const int condition_result = pthread_cond_destroy(&condition);
    const int mutex_result = pthread_mutex_destroy(&mutex);
    return condition_result == 0 && mutex_result == 0;
  };
  pthread_key_t key;
  if (pthread_key_create(&key, Reclaim) != 0) {
    close_sync();
    return -271;
  }
  int parent_value = 1;
  if (pthread_setspecific(key, &parent_value) != 0 ||
      pthread_getspecific(key) != &parent_value) {
    pthread_key_delete(key);
    close_sync();
    return -272;
  }
  std::atomic<int> child_result{0};
  std::thread child([&] {
    if (pthread_mutex_lock(&mutex) != 0) {
      child_result.store(-279, std::memory_order_relaxed);
      return;
    }
    if (pthread_getspecific(key) != nullptr ||
        pthread_setspecific(key, &child_result) != 0 ||
        pthread_getspecific(key) != &child_result) {
      child_result.store(-273, std::memory_order_relaxed);
    }
    const int signaled = pthread_cond_signal(&condition);
    const int unlocked = pthread_mutex_unlock(&mutex);
    if (signaled != 0 || unlocked != 0) {
      child_result.store(-279, std::memory_order_relaxed);
    }
  });
  child.join();
  const int result = child_result.load(std::memory_order_relaxed);
  if (result != 0 || pthread_getspecific(key) != &parent_value ||
      reclaimed.load(std::memory_order_relaxed) != 1) {
    pthread_key_delete(key);
    close_sync();
    return -274;
  }
  if (pthread_key_delete(key) != 0) {
    close_sync();
    return -275;
  }
  if (!close_sync()) {
    return -280;
  }
#ifdef SYMBIAN_RUNTIME_CHANGED_TLS
  return -276;
#else
  return 0;
#endif
}
