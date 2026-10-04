// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <algorithm>
#include <array>
#include <cstdint>
#include <thread>
#include <utility>

#include "gtest/gtest.h"
#include "native_camera.h"
#include "native_display.h"
#include "native_power.h"
#include "native_storage.h"
#include "symbian/api/camera/camera.h"
#include "symbian/api/display/display.h"
#include "symbian/api/power/power.h"
#include "symbian/api/storage/storage.h"
#include "symbian/api/system/counters.h"

namespace {
int tick_period = 1000;
std::uint32_t tick_count = 42;
int fast_frequency = 32768;
std::uint32_t fast_count = 123;
symbian::api::power::NativePowerReading power_reading;
symbian::api::display::NativeDisplayReading display_reading;
alignas(16) unsigned char file_token[16];
alignas(16) unsigned char directory_token[16];
int file_close_count = 0;
int simulated_file_size = 3;
int directory_close_count = 0;
int directory_next_count = 0;
int writable_open_mode = -1;
int write_offset = -1;
int written_length = 0;
int flush_count = 0;
int available_cameras = 0;
}  // namespace

namespace symbian::api::camera {
extern "C" int SymbianDeviceCameraCount() {
  return available_cameras;
}

}  // namespace symbian::api::camera

namespace symbian::api::storage {
extern "C" int SymbianDeviceFileOpen(const char16_t*, int,
                                     NativeFile** output) {
  *output = reinterpret_cast<NativeFile*>(file_token);
  return 0;
}

extern "C" int SymbianDeviceWritableFileOpen(const char16_t*, int, int mode,
                                             NativeFile** output) {
  writable_open_mode = mode;
  *output = reinterpret_cast<NativeFile*>(file_token);
  return 0;
}

extern "C" int SymbianDeviceFileWriteAt(NativeFile*, int offset,
                                        const unsigned char* source,
                                        int length) {
  write_offset = offset;
  written_length = length;
  if (simulated_file_size > 3) {
    return length > 0 && source[0] == 'A' ? 0 : -6;
  }
  return length == 3 && source[0] == 'A' && source[2] == 'C' ? 0 : -6;
}

extern "C" int SymbianDeviceFileFlush(NativeFile*) {
  ++flush_count;
  return 0;
}

extern "C" int SymbianDeviceCreateDirectories(const char16_t*, int) {
  return 0;
}

extern "C" int SymbianDeviceFileSize(NativeFile*, int* size) {
  *size = simulated_file_size;
  return 0;
}

extern "C" int SymbianDeviceFileReadAt(NativeFile*, int offset,
                                       unsigned char* output, int capacity,
                                       int* bytes_read) {
  if (simulated_file_size > 3) {
    if (offset < 0 || capacity <= 0 || offset >= simulated_file_size) {
      return -6;
    }
    *bytes_read = std::min(capacity, simulated_file_size - offset);
    std::fill_n(output, *bytes_read, 'A');
    return 0;
  }
  if (offset != 0 || capacity < 3) {
    return -6;
  }
  output[0] = 'A';
  output[1] = 'B';
  output[2] = 'C';
  *bytes_read = 3;
  return 0;
}

extern "C" void SymbianDeviceFileClose(NativeFile* file) {
  if (file != nullptr) {
    ++file_close_count;
  }
}

extern "C" int SymbianDeviceDirectoryOpen(const char16_t*, int,
                                          NativeDirectory** output) {
  *output = reinterpret_cast<NativeDirectory*>(directory_token);
  return 0;
}

extern "C" int SymbianDeviceDirectoryNext(NativeDirectory*,
                                          NativeDirectoryEntry* output,
                                          bool* end) {
  if (directory_next_count++ > 0) {
    *end = true;
    return 0;
  }
  output->name[0] = u'x';
  output->name_length = 1;
  output->is_directory = false;
  output->size_bytes = 3;
  *end = false;
  return 0;
}

extern "C" void SymbianDeviceDirectoryClose(NativeDirectory* directory) {
  if (directory != nullptr) {
    ++directory_close_count;
  }
}
}  // namespace symbian::api::storage

namespace symbian::api::power {
extern "C" void SymbianDeviceReadPower(NativePowerReading* reading) {
  *reading = power_reading;
}
}  // namespace symbian::api::power

namespace symbian::api::display {
extern "C" void SymbianDeviceReadPrimaryDisplay(NativeDisplayReading* reading) {
  *reading = display_reading;
}
}  // namespace symbian::api::display

extern "C" int SymbianRuntimeTickPeriodMicros() {
  return tick_period;
}

extern "C" unsigned int SymbianRuntimeTickCount() {
  return tick_count;
}

extern "C" int SymbianRuntimeFastCounterFrequency() {
  return fast_frequency;
}

extern "C" unsigned int SymbianRuntimeFastCounter() {
  return fast_count;
}

TEST(DeviceApiTest, CounterReadingsKeepNativeUnits) {
  tick_period = 1000;
  tick_count = 0xfffffff0u;
  auto tick = symbian::api::system::ReadTickCounter();
  ASSERT_TRUE(tick.ok()) << tick.status();
  EXPECT_EQ(tick->count, 0xfffffff0u);
  EXPECT_EQ(tick->period.count(), 1000);

  fast_frequency = 32768;
  fast_count = 123;
  auto fast = symbian::api::system::ReadFastCounter();
  ASSERT_TRUE(fast.ok()) << fast.status();
  EXPECT_EQ(fast->count, 123u);
  EXPECT_EQ(fast->ticks_per_second, 32768u);
}

TEST(DeviceApiTest, MissingCounterMetadataIsAnError) {
  tick_period = -5;  // KErrNotSupported.
  EXPECT_EQ(symbian::api::system::ReadTickCounter().status().code(),
            absl::StatusCode::kUnimplemented);
  tick_period = 0;
  EXPECT_EQ(symbian::api::system::ReadTickCounter().status().code(),
            absl::StatusCode::kFailedPrecondition);

  fast_frequency = -5;
  EXPECT_EQ(symbian::api::system::ReadFastCounter().status().code(),
            absl::StatusCode::kUnimplemented);
  fast_frequency = 0;
  EXPECT_EQ(symbian::api::system::ReadFastCounter().status().code(),
            absl::StatusCode::kFailedPrecondition);
}

TEST(DeviceApiTest, PowerSnapshotPreservesPartialAndUnknownReadings) {
  power_reading = {};
  power_reading.power_good_result = 0;
  power_reading.power_good = 1;
  power_reading.external_power_result = -5;
  power_reading.battery_result = 0;
  power_reading.battery = 3;
  auto snapshot = symbian::api::power::ReadPowerSnapshot();
  ASSERT_TRUE(snapshot.ok()) << snapshot.status();
  ASSERT_TRUE(snapshot->power_good.has_value());
  EXPECT_TRUE(*snapshot->power_good);
  EXPECT_FALSE(snapshot->external_power.has_value());
  ASSERT_TRUE(snapshot->battery.has_value());
  EXPECT_EQ(*snapshot->battery, symbian::api::power::BatteryCondition::kGood);

  power_reading.power_good = 2;
  power_reading.battery = 99;
  EXPECT_EQ(symbian::api::power::ReadPowerSnapshot().status().code(),
            absl::StatusCode::kFailedPrecondition);
}

TEST(DeviceApiTest, DisplayRequiresPixelsAndKeepsOptionalTwips) {
  display_reading = {};
  display_reading.width_result = 0;
  display_reading.height_result = 0;
  display_reading.width_pixels = 640;
  display_reading.height_pixels = 360;
  auto geometry = symbian::api::display::ReadPrimaryDisplayGeometry();
  ASSERT_TRUE(geometry.ok()) << geometry.status();
  EXPECT_EQ(geometry->width_pixels, 640);
  EXPECT_EQ(geometry->height_pixels, 360);
  EXPECT_FALSE(geometry->width_twips.has_value());

  display_reading.width_pixels = 0;
  EXPECT_EQ(symbian::api::display::ReadPrimaryDisplayGeometry().status().code(),
            absl::StatusCode::kFailedPrecondition);
}

TEST(DeviceApiTest, StorageOwnsHandlesAndStreamsBoundedResults) {
  file_close_count = 0;
  directory_close_count = 0;
  directory_next_count = 0;
  {
    auto opened = symbian::api::storage::ReadOnlyFile::Open(u"Z:\\data\\a.bin");
    ASSERT_TRUE(opened.ok()) << opened.status();
    auto file = std::move(*opened);
    auto size = file.Size();
    ASSERT_TRUE(size.ok()) << size.status();
    EXPECT_EQ(*size, 3u);
    std::array<std::byte, 4> bytes{};
    auto read = file.ReadAt(0, bytes);
    ASSERT_TRUE(read.ok()) << read.status();
    EXPECT_EQ(*read, 3u);
    EXPECT_EQ(bytes[0], std::byte{'A'});
  }
  EXPECT_EQ(file_close_count, 1);
  {
    auto opened = symbian::api::storage::DirectoryReader::Open(u"Z:\\data");
    ASSERT_TRUE(opened.ok()) << opened.status();
    auto reader = std::move(*opened);
    auto first = reader.Next();
    ASSERT_TRUE(first.ok()) << first.status();
    ASSERT_TRUE(first->has_value());
    EXPECT_EQ(first->value().name, u"x");
    EXPECT_EQ(first->value().size_bytes, 3u);
    auto end = reader.Next();
    ASSERT_TRUE(end.ok()) << end.status();
    EXPECT_FALSE(end->has_value());
    const int calls_before_cancel = directory_next_count;
    std::thread cancellation([&reader] { reader.Cancel(); });
    cancellation.join();
    EXPECT_EQ(reader.Next().status().code(), absl::StatusCode::kCancelled);
    EXPECT_EQ(directory_next_count, calls_before_cancel);
  }
  EXPECT_EQ(directory_close_count, 1);
  EXPECT_EQ(
      symbian::api::storage::ReadOnlyFile::Open(u"relative").status().code(),
      absl::StatusCode::kInvalidArgument);
}

TEST(DeviceApiTest, WritableStorageMakesReplacementExplicit) {
  writable_open_mode = -1;
  file_close_count = 0;
  flush_count = 0;
  EXPECT_TRUE(symbian::api::storage::CreateDirectories(u"C:\\data\\test").ok());
  {
    auto opened = symbian::api::storage::WritableFile::Open(
        u"C:\\data\\test\\sample.bin",
        symbian::api::storage::WriteMode::kCreateNew);
    ASSERT_TRUE(opened.ok()) << opened.status();
    EXPECT_EQ(writable_open_mode, 0);
    auto file = std::move(*opened);
    const std::array<std::byte, 3> source = {std::byte{'A'}, std::byte{'B'},
                                             std::byte{'C'}};
    EXPECT_TRUE(file.WriteAt(7, source).ok());
    EXPECT_EQ(write_offset, 7);
    EXPECT_EQ(written_length, 3);
    EXPECT_TRUE(file.Flush().ok());
    EXPECT_EQ(flush_count, 1);
    EXPECT_EQ(file.WriteAt(static_cast<std::uint64_t>(1) << 32, source).code(),
              absl::StatusCode::kUnimplemented);
  }
  EXPECT_EQ(file_close_count, 1);
  auto replacing = symbian::api::storage::WritableFile::Open(
      u"C:\\data\\test\\sample.bin",
      symbian::api::storage::WriteMode::kReplaceExisting);
  ASSERT_TRUE(replacing.ok()) << replacing.status();
  EXPECT_EQ(writable_open_mode, 2);
}

TEST(DeviceApiTest, FileCopyYieldsProgressAndChecksCancellation) {
  using symbian::api::storage::FileCopy;
  using symbian::api::storage::WriteMode;
  flush_count = 0;
  auto cancelled =
      FileCopy::Open(u"C:\\data\\source.bin", u"C:\\data\\cancelled.bin",
                     WriteMode::kCreateNew);
  ASSERT_TRUE(cancelled.ok()) << cancelled.status();
  cancelled->Cancel();
  EXPECT_EQ(cancelled->Step().status().code(), absl::StatusCode::kCancelled);
  EXPECT_EQ(cancelled->progress().bytes_copied, 0u);
  EXPECT_EQ(flush_count, 0);

  auto opened = FileCopy::Open(u"C:\\data\\source.bin", u"C:\\data\\copied.bin",
                               WriteMode::kCreateNew);
  ASSERT_TRUE(opened.ok()) << opened.status();
  auto copy = std::move(*opened);
  const auto progress = copy.Step();
  ASSERT_TRUE(progress.ok()) << progress.status();
  EXPECT_EQ(progress->bytes_copied, 3u);
  EXPECT_EQ(progress->total_bytes, 3u);
  EXPECT_TRUE(progress->complete);
  EXPECT_EQ(flush_count, 1);
  EXPECT_TRUE(copy.Step()->complete);
  EXPECT_EQ(flush_count, 1);
}

TEST(DeviceApiTest, FileCopyLimitsEachStepAndStopsBeforeNextChunk) {
  using symbian::api::storage::FileCopy;
  using symbian::api::storage::WriteMode;
  simulated_file_size = 70 * 1024;
  flush_count = 0;
  auto opened = FileCopy::Open(u"C:\\data\\source.bin", u"C:\\data\\copied.bin",
                               WriteMode::kCreateNew);
  ASSERT_TRUE(opened.ok()) << opened.status();
  auto copy = std::move(*opened);
  auto first = copy.Step();
  ASSERT_TRUE(first.ok()) << first.status();
  EXPECT_EQ(first->bytes_copied, 32u * 1024u);
  EXPECT_FALSE(first->complete);
  EXPECT_EQ(written_length, 32 * 1024);
  copy.Cancel();
  EXPECT_EQ(copy.Step().status().code(), absl::StatusCode::kCancelled);
  EXPECT_EQ(copy.progress().bytes_copied, first->bytes_copied);
  EXPECT_EQ(flush_count, 0);
  simulated_file_size = 3;
}

TEST(DeviceApiTest, CameraDiscoveryDoesNotInventDeviceDetails) {
  available_cameras = 0;
  auto empty = symbian::api::camera::DiscoverCameras();
  ASSERT_TRUE(empty.ok()) << empty.status();
  EXPECT_EQ(empty->available_count, 0u);

  available_cameras = 2;
  auto present = symbian::api::camera::DiscoverCameras();
  ASSERT_TRUE(present.ok()) << present.status();
  EXPECT_EQ(present->available_count, 2u);

  available_cameras = -5;
  EXPECT_EQ(symbian::api::camera::DiscoverCameras().status().code(),
            absl::StatusCode::kUnimplemented);
}
