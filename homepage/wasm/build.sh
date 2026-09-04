#!/bin/sh
# Rebuilds malaise.js / malaise.wasm from the interpreter source.
#
# Requires emscripten (Debian/Ubuntu: `apt install emscripten`; anything
# emcc-compatible should do). No custom sysroot setup needed -- run this
# from a clean install.
#
# wasm_entry.c exists because of a codegen quirk in this toolchain: clang
# rewrites a literal `int main(int, char**)` into a symbol named
# __main_argc_argv before the linker ever sees anything called "main", so
# there's no "_main" for the JS glue to call. wasm_entry.c exports
# wasm_run(path, licensed) instead, which forwards into the real entry
# point by hand and lets the page set MALAISE_I_HAVE_A_COMMERCIAL_LICENSE
# from JS before main() ever reads it (getenv() only sees env vars that
# existed before the wasm module finished initializing, which is earlier
# than any JS callback the module gives you).
set -eu
cd "$(dirname "$0")"

emcc -O2 -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
  ../../interpreter/malaise.c wasm_entry.c \
  -o malaise.js \
  -s MODULARIZE=1 -s EXPORT_NAME=createMalaiseModule \
  -s EXPORTED_FUNCTIONS='["_wasm_run"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall","FS"]' \
  -s FORCE_FILESYSTEM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=33554432 \
  -s ENVIRONMENT=web

echo "built malaise.js + malaise.wasm"
