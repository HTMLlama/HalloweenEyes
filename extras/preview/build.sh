#!/bin/sh
# Builds the desktop preview of the eye engine from the same sources the
# Feather uses. Uses the Command Line Tools compiler directly if the Xcode
# shim is unavailable (e.g. Xcode licence not yet accepted).
set -e
cd "$(dirname "$0")"
SRC=../..
CXX=${CXX:-c++}
FLAGS=""
if ! $CXX --version >/dev/null 2>&1; then
  CXX=/Library/Developer/CommandLineTools/usr/bin/clang++
  SDK=$(ls -d /Library/Developer/CommandLineTools/SDKs/MacOSX*.sdk 2>/dev/null | tail -1)
  [ -n "$SDK" ] && FLAGS="-isysroot $SDK"
fi
$CXX $FLAGS -std=c++11 -O2 -Wall -Wextra -Wno-unused-parameter -I$SRC -o preview \
  preview.cpp $SRC/EyeRenderer.cpp $SRC/EyeStyles.cpp $SRC/IrisBaker.cpp \
  $SRC/EyeBehavior.cpp $SRC/Screens.cpp
echo "built $(pwd)/preview"
