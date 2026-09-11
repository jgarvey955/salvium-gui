#!/bin/sh
set -eu

# Homebrew supplies the shared library. Build the static archive from the
# same version and checksum used by the pinned core's depends recipes.
: "${GITHUB_WORKSPACE:?}"
: "${RUNNER_TEMP:?}"
recipe="$GITHUB_WORKSPACE/salvium/contrib/depends/packages/hidapi.mk"
hidapi_version=$(sed -n 's/^$(package)_version=//p' "$recipe")
hidapi_sha256=$(sed -n 's/^$(package)_sha256_hash=//p' "$recipe")
test -n "$hidapi_version"
test -n "$hidapi_sha256"

source_dir="$RUNNER_TEMP/salvium-hidapi"
archive="$RUNNER_TEMP/hidapi-$hidapi_version.tar.gz"
prefix="$GITHUB_WORKSPACE/hidapi-install"
curl --fail --location --retry 3 \
    "https://github.com/libusb/hidapi/archive/refs/tags/hidapi-$hidapi_version.tar.gz" \
    --output "$archive"
printf '%s  %s\n' "$hidapi_sha256" "$archive" | shasum -a 256 --check
mkdir -p "$source_dir"
tar -xzf "$archive" --strip-components=1 -C "$source_dir"
cmake -S "$source_dir" -B "$source_dir/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DBUILD_SHARED_LIBS=OFF \
    -DHIDAPI_BUILD_HIDTEST=OFF
cmake --build "$source_dir/build" --parallel "$(sysctl -n hw.ncpu)"
cmake --install "$source_dir/build"
test -f "$prefix/lib/libhidapi.a"
test -f "$prefix/include/hidapi/hidapi.h"
