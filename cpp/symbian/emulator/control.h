// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYMBIAN_EMULATOR_CONTROL_H_
#define SYMBIAN_EMULATOR_CONTROL_H_

#include <memory>

#include <absl/base/nullability.h>

#include "absl/status/statusor.h"

namespace eka2l1::desktop {
struct emulator;
}

namespace symbian::emulator {

// Owns callbacks on EKA2L1's existing Qt and kernel event loops. There are no
// new worker threads, schedulers, Python callbacks or Python reference holders.
class ControlServer {
 public:
  ~ControlServer();
  ControlServer(const ControlServer&) = delete;
  ControlServer& operator=(const ControlServer&) = delete;

  // Empty socket_path disables control. Enabled sockets require an existing
  // private directory, and only expose emulator capture, bounded input and
  // status.
  static absl::StatusOr<std::unique_ptr<ControlServer>> Start(
      eka2l1::desktop::emulator* absl_nullable state,
      const char* absl_nullable socket_path);

  // Called on the Qt loop before stopping EKA2L1's workers/kernel. Detaches
  // callbacks and saves the final status beside the socket, without overwrites.
  absl::Status Stop();

 private:
  struct Impl;
  explicit ControlServer(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace symbian::emulator
#endif  // SYMBIAN_EMULATOR_CONTROL_H_
