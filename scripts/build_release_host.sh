#!/usr/bin/env bash
# Copyright 2026 The Symbian SDK Authors.
# Licensed under the Apache License, Version 2.0.
# Run once per cibuildwheel host/container, then reuse the installed core for
# every CPython binding. This archive is the host component, not a native SDK.
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
prefix=${SYMBIAN_PREBUILT_HOST_SDK:?Set the shared host SDK install prefix}
version=$(cat "$root/VERSION")
case "$(uname -s):${SYMBIAN_WHEEL_ARCH:-$(uname -m)}" in
  Darwin:arm64|Darwin:aarch64) platform=macos; arch=arm64 ;;
  Darwin:x86_64|Darwin:amd64) platform=macos; arch=x86_64 ;;
  Linux:aarch64|Linux:arm64) platform=linux; arch=aarch64 ;;
  Linux:x86_64|Linux:amd64) platform=linux; arch=x86_64 ;;
  *) echo "Unsupported release host" >&2; exit 2 ;;
esac
python3 -m pip install 'cmake>=3.28,<5' 'ninja>=1.12,<2'
if [[ "${SYMBIAN_REUSE_HOST_SDK:-false}" != true ]]; then
  "$root/scripts/bootstrap_wheel_deps.sh"
  build="$root/build/release-host-${platform}-${arch}"
  args=()
  if [[ "$platform" == macos ]]; then
    args+=("-DCMAKE_OSX_ARCHITECTURES=$arch"
           "-DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-15.0}")
  fi
  cmake -S "$root" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_INSTALL_LIBDIR=lib -DSYMBIAN_DEPS_PREFIX="$SYMBIAN_DEPS_PREFIX" \
    -DSYMBIAN_BUILD_PYTHON=OFF -DSYMBIAN_BUILD_GUI_EXAMPLE=OFF \
    -DSYMBIAN_BUILD_NATIVE_EXAMPLES=OFF \
    -DSYMBIAN_INSTALL_HOST_SDK=ON -DBUILD_TESTING=ON "${args[@]}"
  cmake --build "$build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
  ctest --test-dir "$build" --output-on-failure
  cmake --install "$build"
else
  echo "Reusing the tested host core for this source/dependency cache key"
fi
python3 "$root/scripts/check_host_sdk.py" "$prefix"
assets=${SYMBIAN_RELEASE_ASSETS:-$root/release-assets}
# cibuildwheel copies /project into the Linux container; /host is the writable
# host mount. Put the shared core archive there so it survives container exit.
if [[ "$platform" == linux && -d /host && -n "${SYMBIAN_RELEASE_ASSETS:-}" ]]; then
  assets="/host$assets"
fi
mkdir -p "$assets"
tar -C "$prefix" -czf \
  "$assets/symbian-host-${version}-${platform}-${arch}.tar.gz" .
