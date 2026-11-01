#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [ -d /workspace/toolchain/usr ]; then
  export PATH="/workspace/toolchain/usr/bin:$PATH"
  export NSISDIR=/workspace/toolchain/usr/share/nsis
fi
mkdir -p dist
(cd src && "${WINDRES:-x86_64-w64-mingw32-windres}" --preprocessor="${CC:-x86_64-w64-mingw32-gcc-posix}" --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED app.rc ../dist/app-res.o)
"${CC:-x86_64-w64-mingw32-gcc-posix}" -std=c11 -Os -Wall -Wextra -Werror -municode -mwindows -static -o dist/ScheduledClick.exe src/main.c dist/app-res.o -lcomctl32 -luser32 -lgdi32
(cd installer && "${MAKENSIS:-makensis}" setup.nsi)
cp "dist/Time Click.exe" dist/Setup.exe
(cd dist && sha256sum ScheduledClick.exe "Time Click.exe" Setup.exe > SHA256SUMS)
