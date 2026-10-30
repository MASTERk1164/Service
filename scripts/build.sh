#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [ -d /workspace/toolchain/usr ]; then
  export PATH="/workspace/toolchain/usr/bin:$PATH"
  export NSISDIR=/workspace/toolchain/usr/share/nsis
fi
mkdir -p dist
"${CC:-x86_64-w64-mingw32-gcc-posix}" -std=c11 -Os -Wall -Wextra -Werror -municode -mwindows -static -o dist/ScheduledClick.exe src/main.c -lcomctl32 -luser32 -lgdi32
(cd installer && "${MAKENSIS:-makensis}" setup.nsi)
sha256sum dist/ScheduledClick.exe dist/Setup.exe > dist/SHA256SUMS
