#!/bin/sh
set -eu

qml_image=${1:-salvium:build-env-linux}
qml_binary=${2:-build/release/bin/salvium-wallet-gui}
qml_workspace=${GITHUB_WORKSPACE:-$(pwd)}

# The GCC 16 image uses a newer glibc than the GitHub host. Run the binary
# with its build image's libraries, a private X display, and temporary settings.
exec docker run --rm --init --network none \
  -e QT_QUICK_BACKEND=software -e HOME=/tmp/salvium-qml-test \
  -v "$qml_workspace:/salvium-gui:ro" -w /tmp "$qml_image" \
  sh -c 'mkdir -p "$HOME"; exec xvfb-run -a "$@"' sh \
  "/salvium-gui/$qml_binary" --test-qml
