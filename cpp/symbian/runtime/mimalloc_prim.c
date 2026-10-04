/* Copyright 2026 Symbian SDK Authors. Licensed under Apache-2.0. */
#include <pthread.h>
#include <string.h>

#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/prim-tls.h"
#include "mimalloc/prim.h"

#undef pthread_getspecific
#undef pthread_setspecific
extern int pthread_setspecific(pthread_key_t key, const void* value);
extern void SymbianMimallocForgetThread(void);
extern void SymbianMimallocCacheEnter(pthread_key_t default_key,
                                      pthread_key_t cached_key);
static pthread_key_t mi_symbian_exit_key = MI_PTHREAD_KEY_INVALID;

void SymbianRuntimeMimallocEnterThread(void) {
  _mi_tls_slots_init();
  const pthread_key_t default_key =
      mi_atomic_load_relaxed(&_mi_theap_default_key);
  const pthread_key_t cached_key =
      mi_atomic_load_relaxed(&_mi_theap_cached_key);
  SymbianMimallocCacheEnter(default_key, cached_key);
}

void SymbianRuntimeMimallocLeaveThread(void) {
  if (mi_symbian_exit_key != MI_PTHREAD_KEY_INVALID) {
    pthread_setspecific(mi_symbian_exit_key, NULL);
  }
  mi_thread_done();
  SymbianMimallocForgetThread();
}

extern int SymbianRuntimeMimallocPageSize(void);
extern void* SymbianRuntimeMimallocReserve(size_t bytes, size_t alignment,
                                           bool commit);
extern int SymbianRuntimeMimallocRelease(void* pages);
extern int SymbianRuntimeMimallocCommit(void* pages, size_t bytes);
extern int SymbianRuntimeMimallocDecommit(void* pages, size_t bytes);
extern void SymbianRuntimeMimallocYield(void);
extern long long SymbianRuntimeMimallocClockMillis(void);

void* SymbianRuntimeMimallocAllocate(unsigned int size) {
  return mi_malloc((size_t)size);
}

void SymbianRuntimeMimallocFree(void* pointer) {
  mi_free(pointer);
}

void SymbianRuntimeMimallocCollect(void) {
  mi_collect(true);
}

void _mi_prim_mem_init(mi_os_mem_config_t* config) {
  memset(config, 0, sizeof(*config));
  const int page_size = SymbianRuntimeMimallocPageSize();
  config->page_size = page_size > 0 ? (size_t)page_size : 4096;
  config->alloc_granularity = config->page_size;
  config->virtual_address_bits = 32;
  config->has_overcommit = false;
  config->has_partial_free = false;
  config->has_virtual_reserve = true;
}

int _mi_prim_alloc(void* hint_addr, size_t size, size_t try_alignment,
                   bool commit, bool allow_large, bool* is_large, bool* is_zero,
                   void** addr) {
  MI_UNUSED(hint_addr);
  MI_UNUSED(allow_large);
  *is_large = false;
  *is_zero = false;
  *addr = SymbianRuntimeMimallocReserve(size, try_alignment, commit);
  return *addr == NULL ? 1 : 0;
}

int _mi_prim_free(void* addr, size_t size) {
  MI_UNUSED(size);
  return SymbianRuntimeMimallocRelease(addr);
}

int _mi_prim_commit(void* addr, size_t size, bool* is_zero) {
  *is_zero = false;
  return SymbianRuntimeMimallocCommit(addr, size);
}

int _mi_prim_decommit(void* addr, size_t size, bool* needs_recommit) {
  *needs_recommit = true;
  return SymbianRuntimeMimallocDecommit(addr, size);
}

int _mi_prim_reset(void* addr, size_t size) {
  MI_UNUSED(addr);
  MI_UNUSED(size);
  return 0;
}

int _mi_prim_reuse(void* addr, size_t size) {
  MI_UNUSED(addr);
  MI_UNUSED(size);
  return 0;
}

int _mi_prim_protect(void* addr, size_t size, bool protect) {
  MI_UNUSED(addr);
  MI_UNUSED(size);
  MI_UNUSED(protect);
  return 1;
}

int _mi_prim_alloc_huge_os_pages(void* hint_addr, size_t size, int numa_node,
                                 bool* is_zero, void** addr) {
  MI_UNUSED(hint_addr);
  MI_UNUSED(size);
  MI_UNUSED(numa_node);
  *is_zero = false;
  *addr = NULL;
  return 1;
}

size_t _mi_prim_numa_node(void) {
  return 0;
}

size_t _mi_prim_numa_node_count(void) {
  return 1;
}

mi_msecs_t _mi_prim_clock_now(void) {
  return (mi_msecs_t)SymbianRuntimeMimallocClockMillis();
}

void _mi_prim_process_info(mi_process_info_t* info) {
  memset(info, 0, sizeof(*info));
}

void _mi_prim_out_stderr(const char* msg) {
  MI_UNUSED(msg);
}

int _mi_prim_getenv(const char* name, char* result, size_t result_size) {
  MI_UNUSED(name);
  MI_UNUSED(result);
  MI_UNUSED(result_size);
  return 0;
}

bool _mi_prim_random_buf(void* buf, size_t len) {
  MI_UNUSED(buf);
  MI_UNUSED(len);
  return false;
}

static void mi_symbian_thread_done(void* value) {
  if (value != NULL) {
    _mi_thread_done((mi_theap_t*)value);
  }
  SymbianMimallocForgetThread();
}

void _mi_prim_thread_init_auto_done(void) {
  pthread_key_create(&mi_symbian_exit_key, mi_symbian_thread_done);
}

void _mi_prim_thread_done_auto_done(void) {
  if (mi_symbian_exit_key != MI_PTHREAD_KEY_INVALID) {
    pthread_key_delete(mi_symbian_exit_key);
    mi_symbian_exit_key = MI_PTHREAD_KEY_INVALID;
  }
}

void _mi_prim_thread_associate_default_theap(mi_theap_t* theap) {
  if (mi_symbian_exit_key != MI_PTHREAD_KEY_INVALID) {
    pthread_setspecific(mi_symbian_exit_key, theap);
  }
}

bool _mi_prim_thread_is_in_threadpool(void) {
  return false;
}

void _mi_prim_thread_yield(void) {
  SymbianRuntimeMimallocYield();
}

static void __attribute__((constructor(101))) mi_symbian_process_attach(void) {
  _mi_auto_process_init();
  SymbianRuntimeMimallocEnterThread();
}

static void __attribute__((destructor(101))) mi_symbian_process_detach(void) {
  _mi_auto_process_done();
  SymbianMimallocForgetThread();
}

bool _mi_is_redirected(void) {
  return false;
}

bool _mi_allocator_init(const char** message) {
  if (message != NULL) {
    *message = NULL;
  }
  return true;
}

void _mi_allocator_done(void) {}
