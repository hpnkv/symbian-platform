// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_MEDIA_MIDI_OUTPUT_H_
#define SYMBIAN_API_MEDIA_MIDI_OUTPUT_H_

#include "absl/base/nullability.h"
#include "absl/status/status.h"

namespace symbian::api::media {

/**
 * @brief Optional native MIDI note output on a dedicated service thread.
 *
 * Start queues service construction without blocking the caller. PlayNote
 * admits a note only after the service becomes available. The bounded queue
 * reports saturation. Destruction requests shutdown without waiting for a
 * firmware service that may be stalled.
 */
class MidiOutput final {
 public:
  MidiOutput();
  MidiOutput(const MidiOutput&) = delete;
  MidiOutput& operator=(const MidiOutput&) = delete;
  ~MidiOutput();

  absl::Status Start();
  absl::Status PlayNote(int note, int duration_ms, int velocity);
  bool available() const;
  absl::Status status() const;

 private:
  struct Impl;
  Impl* absl_nullable impl_ = nullptr;
};

}  // namespace symbian::api::media

#endif  // SYMBIAN_API_MEDIA_MIDI_OUTPUT_H_
