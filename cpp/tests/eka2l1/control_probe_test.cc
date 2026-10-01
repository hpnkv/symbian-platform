// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QTemporaryDir>
#include <string>

#include <gtest/gtest.h>

#include "symbian/emulator/control.h"

namespace {

using symbian::emulator::ControlServer;

TEST(ControlProbeTest, DisabledEndpointNeedsNoEmulator) {
  auto absent = ControlServer::Start(nullptr, nullptr);
  ASSERT_TRUE(absent.ok()) << absent.status();
  EXPECT_EQ(absent->get(), nullptr);
  auto empty = ControlServer::Start(nullptr, "");
  ASSERT_TRUE(empty.ok()) << empty.status();
  EXPECT_EQ(empty->get(), nullptr);
}

TEST(ControlProbeTest, RejectsRelativeAndOversizedSocketPaths) {
  EXPECT_EQ(ControlServer::Start(nullptr, "relative.sock").status().code(),
            absl::StatusCode::kInvalidArgument);
  const std::string oversized = "/tmp/" + std::string(101, 'x');
  EXPECT_EQ(ControlServer::Start(nullptr, oversized.c_str()).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ControlProbeTest, RejectsNonPrivateParentBeforeAccessingState) {
  QTemporaryDir directory("/tmp/symbian-control-XXXXXX");
  ASSERT_TRUE(directory.isValid());
  ASSERT_TRUE(QFile::setPermissions(
      directory.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                            QFile::ReadGroup));
  const auto endpoint = (directory.path() + "/control.sock").toStdString();
  EXPECT_EQ(ControlServer::Start(nullptr, endpoint.c_str()).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_FALSE(QFile::exists(QString::fromStdString(endpoint)));
}

TEST(ControlProbeTest, MissingEmulatorDoesNotCreateSocket) {
  QTemporaryDir directory("/tmp/symbian-control-XXXXXX");
  ASSERT_TRUE(directory.isValid());
  ASSERT_TRUE(QFile::setPermissions(directory.path(), QFile::ReadOwner |
                                                          QFile::WriteOwner |
                                                          QFile::ExeOwner));
  const auto endpoint = (directory.path() + "/control.sock").toStdString();
  EXPECT_EQ(ControlServer::Start(nullptr, endpoint.c_str()).status().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(QFile::exists(QString::fromStdString(endpoint)));
}

}  // namespace
