#!/usr/bin/env bash
# Incremental emscripten build for web target.
# M5GFX object files are cached in web/lib/ and only recompiled when sources change.
# Usage: bash scripts/build_web.sh [--release]

set -e

LIBDIR=.pio/build/web
OUTDIR=web
LIBSRC=".pio/libdeps/native/M5GFX/src"
INCLUDES="-I $LIBSRC"
DEFINES="-D __EMSCRIPTEN__ -D LGFX_USE_SDL -D M5GFX_BOARD=board_M5Cardputer -s USE_SDL=2"
EMFLAGS="-s USE_SDL=2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s FORCE_FILESYSTEM=1 -lidbfs.js --pre-js scripts/pre.js -s EXPORTED_FUNCTIONS=['_main','_malloc','_free'] -s EXPORTED_RUNTIME_METHODS=['HEAPU8'] -Wl,--export-if-defined=cardputerLoraInject -Wl,--export-if-defined=cardputerGpsSet -Wl,--export-if-defined=cardputerMotionSet"

# In release mode add SINGLE_FILE so the page is self-contained
if [[ "$1" == "--release" ]]; then
  EMFLAGS="$EMFLAGS -s SINGLE_FILE=1"
  OUT="$OUTDIR/index.mjs"
else
  # Dev: separate .wasm file so browser can cache it between rebuilds
  OUT="$OUTDIR/index.mjs"
fi

mkdir -p "$LIBDIR"

echo "==> Building M5GFX library objects (skipping up-to-date files)..."
OBJS=()
for src in \
  "$LIBSRC/M5GFX.cpp" \
  "$LIBSRC/lgfx/v1/"*.cpp \
  "$LIBSRC/lgfx/v1/misc/"*.cpp \
  "$LIBSRC/lgfx/v1/panel/"*.cpp \
  "$LIBSRC/lgfx/v1/platforms/sdl/"*.cpp
do
  obj="$LIBDIR/$(echo "$src" | tr '/' '_').o"
  if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
    echo "  CC  $src"
    emcc -c "$src" -o "$obj" $INCLUDES $DEFINES -std=c++14 -O2
  fi
  OBJS+=("$obj")
done

echo "==> Compiling src/..."
SRC_OBJS=()
for src in src/*.cpp; do
  obj="$LIBDIR/$(echo "$src" | tr '/' '_').o"
  echo "  CC  $src"
  emcc -c "$src" -o "$obj" $INCLUDES $DEFINES -std=c++14 -O0
  SRC_OBJS+=("$obj")
done

echo "==> Linking -> $OUT"
emcc "${OBJS[@]}" "${SRC_OBJS[@]}" -o "$OUT" $EMFLAGS

echo "==> Done."
