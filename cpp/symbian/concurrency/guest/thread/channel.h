// Copyright 2026 The Action Engine Authors and the Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.
// Guest adaptation of A11 cpp/thread/thread/channel.h at
// fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b.

#ifndef THREAD_FIBER_CHANNEL_H_
#define THREAD_FIBER_CHANNEL_H_

#include <cstddef>
#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <absl/base/nullability.h>

#include "absl/container/inlined_vector.h"
#include "thread/boost_primitives.h"
#include "thread/fiber.h"
#include "thread/select.h"

namespace thread {

template <typename T>
requires std::is_move_assignable_v<T> class Channel;

template <typename T>
class Reader {
 public:
  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  bool Read(T* absl_nonnull out) { return channel_->Read(out); }

  Case OnRead(T* absl_nonnull out, bool* absl_nonnull ok) {
    return channel_->OnRead(out, ok);
  }

 private:
  friend class Channel<T>;

  explicit Reader(Channel<T>* absl_nonnull channel) : channel_(channel) {}

  Channel<T>* absl_nonnull channel_;
};

template <typename T>
class Writer {
 public:
  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

  void Write(T&& item) { channel_->Write(std::move(item)); }

  void Write(const T& item) { channel_->Write(item); }

  Case OnWrite(T&& item) { return channel_->OnWrite(std::move(item)); }

  Case OnWrite(const T& item) { return channel_->OnWrite(item); }

  bool WriteUnlessCancelled(T&& item) {
    return !thread::Cancelled() &&
           Select({thread::OnCancel(), OnWrite(std::move(item))}) == 1;
  }

  bool WriteUnlessCancelled(const T& item) {
    return !thread::Cancelled() &&
           Select({thread::OnCancel(), OnWrite(item)}) == 1;
  }

  void Close() { channel_->Close(); }

 private:
  friend class Channel<T>;

  explicit Writer(Channel<T>* absl_nonnull channel) : channel_(channel) {}

  Channel<T>* absl_nonnull channel_;
};

// A11-compatible selectable buffered/rendezvous channel. Every transfer and
// selection decision is made under the channel and selector locks. Wakes run
// after those locks are released, so a ready fiber can reenter the channel.
template <typename T>
requires std::is_move_assignable_v<T> class Channel {
  enum class Transfer { kCopy, kMove };
  inline static constexpr Transfer kCopy = Transfer::kCopy;
  inline static constexpr Transfer kMove = Transfer::kMove;

  class ReadSelectable final : public internal::Selectable {
   public:
    explicit ReadSelectable(Channel* absl_nonnull channel)
        : channel_(channel) {}

    bool Handle(internal::CaseInSelectClause* absl_nonnull state,
                bool enqueue) override {
      return channel_->HandleRead(state, enqueue);
    }

    void Unregister(internal::CaseInSelectClause* absl_nonnull state) override {
      channel_->Unregister(&channel_->readers_, state);
    }

   private:
    Channel* absl_nonnull channel_;
  };

  class WriteSelectable final : public internal::Selectable {
   public:
    explicit WriteSelectable(Channel* absl_nonnull channel)
        : channel_(channel) {}

    bool Handle(internal::CaseInSelectClause* absl_nonnull state,
                bool enqueue) override {
      return channel_->HandleWrite(state, enqueue);
    }

    void Unregister(internal::CaseInSelectClause* absl_nonnull state) override {
      channel_->Unregister(&channel_->writers_, state);
    }

   private:
    Channel* absl_nonnull channel_;
  };

 public:
  explicit Channel(std::size_t capacity)
      : capacity_(capacity),
        reader_(this),
        read_selectable_(this),
        writer_(this),
        write_selectable_(this) {}

  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;

  ~Channel() {
    MutexLock lock(&mu_);
    if (readers_ != nullptr || writers_ != nullptr) {
      std::abort();
    }
  }

  Reader<T>* absl_nonnull reader() { return &reader_; }

  Writer<T>* absl_nonnull writer() { return &writer_; }

  std::size_t length() const {
    MutexLock lock(&mu_);
    return queue_.size();
  }

 private:
  friend class Reader<T>;
  friend class Writer<T>;

  bool Read(T* absl_nonnull out) {
    bool ok = false;
    Select({OnRead(out, &ok)});
    return ok;
  }

  void Write(T&& item) { Select({OnWrite(std::move(item))}); }

  void Write(const T& item) requires std::is_copy_constructible_v<T> {
    Select({OnWrite(item)});
  }

  Case OnRead(T* absl_nonnull out, bool* absl_nonnull ok) {
    return {&read_selectable_, out, ok};
  }

  Case OnWrite(T&& item) { return MakeWriteCase(&item, kMove); }

  Case OnWrite(const T& item) requires std::is_copy_constructible_v<T> {
    return MakeWriteCase(const_cast<T*>(&item), kCopy);
  }

  void Close() { CloseImpl(); }

  using State = internal::CaseInSelectClause;
  using Selector = internal::Selector;
  using Wakes = absl::InlinedVector<std::shared_ptr<Selector>, 4>;

  static T TransferItem(T* absl_nonnull item, Transfer strategy) {
    if (strategy == kMove) {
      return std::move(*item);
    }
    if constexpr (std::is_copy_constructible_v<T>) {
      return *item;
    }
    std::abort();
  }

  static void Wake(Wakes* absl_nonnull wakes) {
    for (const auto& selector : *wakes) {
      selector->cv.Signal();
    }
  }

  static void UnlinkIfQueued(State* absl_nullable* absl_nonnull head,
                             State* absl_nonnull state) {
    if (state->prev != nullptr) {
      internal::UnlinkFromList(head, state);
    }
  }

  static bool LockPair(State* absl_nonnull first, State* absl_nonnull second) {
    Selector* absl_nonnull left = first->selector.get();
    Selector* absl_nonnull right = second->selector.get();
    if (left == right) {
      return false;
    }
    if (std::less<Selector*>{}(right, left)) {
      std::swap(left, right);
    }
    left->mu.Lock();
    right->mu.Lock();
    if (left->picked_case_index == Selector::kNonePicked &&
        right->picked_case_index == Selector::kNonePicked) {
      return true;
    }
    right->mu.Unlock();
    left->mu.Unlock();
    return false;
  }

  static void UnlockPair(State* absl_nonnull first,
                         State* absl_nonnull second) {
    first->selector->mu.Unlock();
    second->selector->mu.Unlock();
  }

  static State* absl_nullable FindMatch(State* absl_nullable head,
                                        State* absl_nonnull counterpart) {
    if (head == nullptr) {
      return nullptr;
    }
    State* absl_nullable current = head;
    do {
      if (LockPair(current, counterpart)) {
        return current;
      }
      current = current->next;
    } while (current != head);
    return nullptr;
  }

  // Called with channel mu held. A queued writer can refill one freed buffer
  // slot, including after the reader's immediate-case transfer.
  void AdmitWriter(Wakes* absl_nonnull wakes) {
    if (writers_ == nullptr || queue_.size() >= capacity_) {
      return;
    }
    State* absl_nullable current = writers_;
    do {
      State* absl_nullable next = current->next;
      MutexLock selector_lock(&current->selector->mu);
      if (current->selector->picked_case_index == Selector::kNonePicked) {
        T* absl_nonnull item = current->GetCase()->template GetArgPtr<T>(0);
        Transfer strategy =
            *current->GetCase()->template GetArgPtr<Transfer>(1);
        queue_.push_back(TransferItem(item, strategy));
        current->TryPick();
        UnlinkIfQueued(&writers_, current);
        wakes->push_back(current->selector);
        return;
      }
      current = next;
    } while (current != writers_ && writers_ != nullptr);
  }

  bool HandleRead(State* absl_nonnull reader, bool enqueue) {
    Wakes wakes;
    bool ready = false;
    {
      MutexLock lock(&mu_);
      T* absl_nonnull out = reader->GetCase()->template GetArgPtr<T>(0);
      bool* absl_nonnull ok = reader->GetCase()->template GetArgPtr<bool>(1);
      if (!queue_.empty()) {
        bool consumed = false;
        {
          MutexLock selector_lock(&reader->selector->mu);
          if (reader->TryPick()) {
            *out = std::move(queue_.front());
            queue_.pop_front();
            *ok = true;
            consumed = true;
          }
        }
        if (consumed) {
          AdmitWriter(&wakes);
        }
        ready = true;
      } else if (State* absl_nullable writer = FindMatch(writers_, reader)) {
        T* absl_nonnull item = writer->GetCase()->template GetArgPtr<T>(0);
        Transfer strategy = *writer->GetCase()->template GetArgPtr<Transfer>(1);
        *out = TransferItem(item, strategy);
        *ok = true;
        reader->TryPick();
        writer->TryPick();
        UnlinkIfQueued(&writers_, writer);
        wakes.push_back(writer->selector);
        UnlockPair(writer, reader);
        ready = true;
      } else {
        MutexLock selector_lock(&reader->selector->mu);
        if (reader->selector->picked_case_index != Selector::kNonePicked) {
          ready = true;
        } else if (closed_) {
          *ok = false;
          reader->TryPick();
          ready = true;
        } else if (enqueue) {
          internal::PushBack(&readers_, reader);
        }
      }
    }
    Wake(&wakes);
    return ready;
  }

  bool HandleWrite(State* absl_nonnull writer, bool enqueue) {
    Wakes wakes;
    bool ready = false;
    {
      MutexLock lock(&mu_);
      if (closed_) {
        std::abort();  // A11's OnWrite on a closed channel is fatal.
      }
      State* absl_nullable reader =
          closed_ ? nullptr : FindMatch(readers_, writer);
      if (reader != nullptr) {
        T* absl_nonnull item = writer->GetCase()->template GetArgPtr<T>(0);
        Transfer strategy = *writer->GetCase()->template GetArgPtr<Transfer>(1);
        T* absl_nonnull out = reader->GetCase()->template GetArgPtr<T>(0);
        bool* absl_nonnull ok = reader->GetCase()->template GetArgPtr<bool>(1);
        *out = TransferItem(item, strategy);
        *ok = true;
        reader->TryPick();
        writer->TryPick();
        UnlinkIfQueued(&readers_, reader);
        wakes.push_back(reader->selector);
        UnlockPair(reader, writer);
        ready = true;
      } else {
        MutexLock selector_lock(&writer->selector->mu);
        if (writer->selector->picked_case_index != Selector::kNonePicked) {
          ready = true;
        } else if (queue_.size() < capacity_) {
          T* absl_nonnull item = writer->GetCase()->template GetArgPtr<T>(0);
          Transfer strategy =
              *writer->GetCase()->template GetArgPtr<Transfer>(1);
          queue_.push_back(TransferItem(item, strategy));
          writer->TryPick();
          ready = true;
        } else if (enqueue) {
          internal::PushBack(&writers_, writer);
        }
      }
    }
    Wake(&wakes);
    return ready;
  }

  void Unregister(State* absl_nullable* absl_nonnull head,
                  State* absl_nonnull state) {
    MutexLock lock(&mu_);
    UnlinkIfQueued(head, state);
  }

  Case MakeWriteCase(T* absl_nonnull item, Transfer strategy) {
    Case result(&write_selectable_);
    result.AddArg(item);
    result.AddArg(strategy == kCopy ? &kCopy : &kMove);
    return result;
  }

  void CloseImpl() {
    Wakes wakes;
    {
      MutexLock lock(&mu_);
      if (closed_ || writers_ != nullptr) {
        std::abort();
      }
      closed_ = true;
      while (readers_ != nullptr) {
        State* absl_nullable state = readers_;
        {
          MutexLock selector_lock(&state->selector->mu);
          if (state->TryPick()) {
            *state->GetCase()->template GetArgPtr<bool>(1) = false;
            wakes.push_back(state->selector);
          }
          internal::UnlinkFromList(&readers_, state);
        }
      }
    }
    Wake(&wakes);
  }

  const std::size_t capacity_;
  mutable Mutex mu_;
  std::deque<T> queue_;
  bool closed_ = false;
  State* absl_nullable readers_ = nullptr;
  State* absl_nullable writers_ = nullptr;
  Reader<T> reader_;
  ReadSelectable read_selectable_;
  Writer<T> writer_;
  WriteSelectable write_selectable_;
};

}  // namespace thread

#endif  // THREAD_FIBER_CHANNEL_H_
