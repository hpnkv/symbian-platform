#!/usr/bin/env bash
# Copyright 2026 The A11 Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Adapted from A11's bootstrap: the host SDK needs static OpenSSL, libusb,
# and Boost.Fiber/Context archives. CMake pins the remaining dependencies.
set -euo pipefail
prefix=${SYMBIAN_DEPS_PREFIX:?Set SYMBIAN_DEPS_PREFIX to an isolated prefix}
arch=${SYMBIAN_WHEEL_ARCH:-$(uname -m)}
host_os=$(uname -s)
deployment_tag=
if [[ "${host_os}" == Darwin ]]; then
  export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-26.0}
  deployment_tag="-macos-${MACOSX_DEPLOYMENT_TARGET}"
fi
case "${host_os}:${arch}" in
  Darwin:x86_64|Darwin:amd64)
    openssl_target=darwin64-x86_64-cc
    boost_arch_args=(toolset=clang target-os=darwin architecture=x86
                     address-model=64 abi=sysv binary-format=mach-o
                     'cxxflags=-arch x86_64' 'linkflags=-arch x86_64') ;;
  Darwin:arm64|Darwin:aarch64)
    openssl_target=darwin64-arm64-cc
    boost_arch_args=(toolset=clang target-os=darwin architecture=arm
                     address-model=64 abi=aapcs binary-format=mach-o
                     'cxxflags=-arch arm64' 'linkflags=-arch arm64') ;;
  Linux:x86_64|Linux:amd64)
    openssl_target=linux-x86_64
    boost_arch_args=(toolset=gcc target-os=linux architecture=x86
                     address-model=64 abi=sysv binary-format=elf
                     cxxflags=-fPIC cflags=-fPIC) ;;
  Linux:aarch64|Linux:arm64)
    openssl_target=linux-aarch64
    boost_arch_args=(toolset=gcc target-os=linux architecture=arm
                     address-model=64 abi=aapcs binary-format=elf
                     cxxflags=-fPIC cflags=-fPIC) ;;
  *) echo "Unsupported dependency target: ${host_os} ${arch}" >&2; exit 2 ;;
esac
# Include host OS, architecture, version and deployment floor in the cache key.
version=3.5.9
libusb_version=1.0.30
boost_version=1.90.0
stamp="${prefix}/.symbian-deps-v3-${host_os}-${arch}-${version}-${libusb_version}-${boost_version}${deployment_tag}"
if [[ -f "${stamp}" && -f "${prefix}/lib/libcrypto.a" &&
      -f "${prefix}/lib/libusb-1.0.a" &&
      -f "${prefix}/lib/libboost_fiber.a" &&
      -f "${prefix}/lib/libboost_context.a" ]]; then exit 0; fi
jobs=${CMAKE_BUILD_PARALLEL_LEVEL:-4}
work=$(mktemp -d "${TMPDIR:-/tmp}/symbian-wheel-deps.XXXXXX")
trap 'rm -rf "${work}"' EXIT
mkdir -p "${prefix}"

download_and_extract() {
  local url=$1
  local archive=$2
  local expected=$3
  local attempt
  # The failures that actually happen here are HTTP ones: GitHub's codeload
  # endpoint rate-limits (429) a host that asks for several archives in a row,
  # and curl's own --retry ignores a 4xx/5xx response. The flag that would fix
  # that, --retry-all-errors, arrived in curl 7.71 and the manylinux_2_28 image
  # ships an older one -- where an unknown option is a hard error, not a warning,
  # so passing it fails every Linux wheel build. Retrying the whole request in
  # the shell needs no flag at all and works on every curl: --fail turns an HTTP
  # error into a non-zero exit, which is what this loop reacts to.
  for attempt in 1 2 3 4 5; do
    if curl --fail --location --retry 3 --connect-timeout 30 \
        --output "${work}/${archive}" "${url}"; then
      local digest
      if command -v sha256sum >/dev/null 2>&1; then
        digest=$(sha256sum "${work}/${archive}" | cut -d ' ' -f 1)
      else
        digest=$(shasum -a 256 "${work}/${archive}" | cut -d ' ' -f 1)
      fi
      [[ "${digest}" == "${expected}" ]] || {
        echo "Source digest mismatch: ${archive}" >&2; return 1;
      }
      tar -xf "${work}/${archive}" -C "${work}"
      return 0
    fi
    if [[ "${attempt}" -lt 5 ]]; then
      echo "download of ${url} failed; retrying in $((attempt * 5))s" >&2
      sleep "$((attempt * 5))"
    fi
  done
  echo "giving up on ${url} after 5 attempts" >&2
  return 1
}


# manylinux's minimal Perl omits modules required by OpenSSL's Configure.
if [[ "${host_os}" == Linux ]]; then
  for module in IPC::Cmd Time::Piece; do
    if ! perl -M"${module}" -e 1 >/dev/null 2>&1; then
      package="perl-${module//::/-}"
      if command -v dnf >/dev/null 2>&1; then dnf install -y "${package}";
      elif command -v yum >/dev/null 2>&1; then yum install -y "${package}";
      else echo "Install Perl ${module} before building dependencies" >&2; exit 1;
      fi
    fi
  done
fi
# Same build contract as A11; updated, hash-pinned OpenSSL LTS source.
download_and_extract   "https://github.com/openssl/openssl/releases/download/openssl-${version}/openssl-${version}.tar.gz"   openssl.tar.gz   603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a
(
  cd "${work}/openssl-${version}"
  ./Configure "${openssl_target}" no-shared no-tests no-module     --prefix="${prefix}" --libdir=lib
  make -j "${jobs}"
  make install_sw
)
download_and_extract \
  "https://github.com/libusb/libusb/releases/download/v${libusb_version}/libusb-${libusb_version}.tar.bz2" \
  libusb.tar.bz2 \
  fea36f34f9156400209595e300840767ab1a385ede1dc7ee893015aea9c6dbaf
(
  cd "${work}/libusb-${libusb_version}"
  ./configure --prefix="${prefix}" --libdir="${prefix}/lib" \
    --disable-shared --enable-static --disable-udev
  make -j "${jobs}"
  make install
)
download_and_extract \
  "https://archives.boost.io/release/${boost_version}/source/boost_1_90_0.tar.bz2" \
  boost.tar.bz2 \
  49551aff3b22cbc5c5a9ed3dbc92f0e23ea50a0f7325b0d198b705e8ee3fc305
(
  cd "${work}/boost_1_90_0"
  ./bootstrap.sh --prefix="${prefix}" \
    --with-libraries=atomic,chrono,context,fiber,thread
  ./b2 -j "${jobs}" "${boost_arch_args[@]}" cxxstd=20 variant=release \
    link=static runtime-link=shared threading=multi install
)
mkdir -p "${prefix}/share"
cp "${work}/libusb-${libusb_version}/COPYING" "${prefix}/COPYING"
cp "${work}/openssl-${version}/LICENSE.txt" "${prefix}/share/symbian-OpenSSL-LICENSE"
cp "${work}/boost_1_90_0/LICENSE_1_0.txt" "${prefix}/share/symbian-Boost-LICENSE"
touch "${stamp}"
