#!/usr/bin/env bash
set -euo pipefail
# Use the existing isolated checkout; do not create worktrees.
mkdir -p /tmp/click-apt/lists/partial /tmp/click-debs /workspace/toolchain
printf 'deb https://deb.debian.org/debian trixie main\n' > /tmp/click-apt/sources.list
opts=(-o Dir::Etc::sourcelist=/tmp/click-apt/sources.list -o Dir::Etc::sourceparts=- -o Dir::State::lists=/tmp/click-apt/lists -o APT::Sandbox::User=agent)
apt "${opts[@]}" update
(cd /tmp/click-debs && apt "${opts[@]}" download gcc-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix-runtime gcc-mingw-w64-base binutils-mingw-w64-x86-64 mingw-w64-x86-64-dev mingw-w64-common nsis nsis-common)
for pkg in /tmp/click-debs/*.deb; do dpkg-deb -x "$pkg" /workspace/toolchain; done
