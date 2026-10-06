// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Adapted from A11 cpp/thread/thread/cases.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.

#ifndef THREAD_FIBER_CASES_H_
#define THREAD_FIBER_CASES_H_

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <type_traits>

#include <absl/base/nullability.h>

#include "absl/container/inlined_vector.h"
#include "absl/time/time.h"
#include "thread/boost_primitives.h"

namespace thread {
namespace internal {

template <typename T>
concept IsPointer = std::is_pointer_v<T>;

template <typename T>
concept IsNonConstPointer =
    IsPointer<T> && !std::is_const_v<std::remove_pointer_t<T>>;

template <typename T>
concept IsConstPointer =
    IsPointer<T> && std::is_const_v<std::remove_pointer_t<T>>;

struct Selector {
  static constexpr int kNonePicked = -1;

  bool TryPick(int index) {
    if (picked_case_index != kNonePicked) {
      return false;
    }
    picked_case_index = index;
    return true;
  }

  // The caller holds mu. Convert the wall deadline once; successive waits
  // use only monotonic elapsed time.
  bool WaitForPick(absl::Time deadline = absl::InfiniteFuture());

  Mutex mu;
  CondVar cv;
  int picked_case_index = kNonePicked;
};

class Selectable;
}  // namespace internal

struct [[nodiscard]] Case {
  internal::Selectable* absl_nonnull selectable;
  absl::InlinedVector<void* absl_nullable, 2> arguments;

  Case(internal::Selectable* absl_nonnull selectable)
      : selectable(selectable) {}

  Case(internal::Selectable* absl_nonnull selectable,
       internal::IsPointer auto... args)
      : selectable(selectable) {
    (arguments.push_back(const_cast<void*>(static_cast<const void*>(args))),
     ...);
  }

  Case(const Case&) = default;
  Case& operator=(const Case&) = default;

  template <typename... Args>
  Case(bool, Args...) = delete;

  template <typename T>
  void AddArg(const T* absl_nonnull arg) {
    arguments.push_back(const_cast<void*>(static_cast<const void*>(arg)));
  }

  template <typename T>
  void AddArg(T* absl_nonnull arg) {
    arguments.push_back(static_cast<void*>(arg));
  }

  void* absl_nullable GetArgPtr(int index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= arguments.size()) {
      std::abort();
    }
    return arguments[static_cast<std::size_t>(index)];
  }

  template <typename T>
  T* absl_nullable GetArgPtr(int index) const {
    return static_cast<T*>(GetArgPtr(index));
  }

  std::size_t GetNumArgs() const { return arguments.size(); }
};

using CaseArray = absl::InlinedVector<Case, 4>;

namespace internal {
struct CaseInSelectClause {
  const Case* absl_nullable case_ptr = nullptr;
  int index = -1;
  std::shared_ptr<Selector> selector;
  CaseInSelectClause* absl_nullable prev = nullptr;
  CaseInSelectClause* absl_nullable next = nullptr;

  const Case* absl_nullable GetCase() const { return case_ptr; }

  bool TryPick() { return selector->TryPick(index); }

  bool Handle(bool enqueue);
  void Unregister();
};

using CaseStateArray = absl::InlinedVector<CaseInSelectClause, 4>;

class Selectable {
 public:
  virtual ~Selectable() = default;
  virtual bool Handle(CaseInSelectClause* absl_nonnull case_state,
                      bool enqueue) = 0;
  virtual void Unregister(CaseInSelectClause* absl_nonnull case_state) = 0;
};

inline bool CaseInSelectClause::Handle(bool enqueue) {
  return GetCase()->selectable->Handle(this, enqueue);
}

inline void CaseInSelectClause::Unregister() {
  GetCase()->selectable->Unregister(this);
}

inline void PushBack(CaseInSelectClause* absl_nullable* absl_nullable head,
                     CaseInSelectClause* absl_nonnull element) {
  if (element->prev != nullptr) {
    std::abort();
  }
  if (*head == nullptr) {
    element->next = element;
    element->prev = element;
    *head = element;
    return;
  }
  element->next = *head;
  element->prev = (*head)->prev;
  element->prev->next = element;
  element->next->prev = element;
}

inline void UnlinkFromList(
    CaseInSelectClause* absl_nullable* absl_nullable head,
    CaseInSelectClause* absl_nonnull element) {
  if (element->prev == nullptr || *head == nullptr) {
    std::abort();
  }
  if (element->next == element) {
    *head = nullptr;
  } else {
    element->next->prev = element->prev;
    element->prev->next = element->next;
    if (*head == element) {
      *head = element->next;
    }
  }
  element->prev = nullptr;
  element->next = nullptr;
}
}  // namespace internal
}  // namespace thread

#endif  // THREAD_FIBER_CASES_H_
