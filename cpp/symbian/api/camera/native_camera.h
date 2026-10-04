// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_API_CAMERA_NATIVE_CAMERA_H_
#define SYMBIAN_API_CAMERA_NATIVE_CAMERA_H_

namespace symbian::api::camera {

// ECam uses a TInt result. Negative values retain their native error code.
extern "C" int SymbianDeviceCameraCount();

}  // namespace symbian::api::camera

#endif  // SYMBIAN_API_CAMERA_NATIVE_CAMERA_H_
