#!/bin/sh
# Compile only. User decides when to execute the benchmark.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
out=${1:-"$root/build-retro-bench/retro-bench-host"}
mkdir -p "$(dirname -- "$out")"
build_id=$(git -C "$root" describe --always --dirty 2>/dev/null || echo unknown)
${CC:-cc} -O2 -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L \
  "-DRETRO_BENCH_BUILD_ID=\"host-$build_id\"" \
  ${CFLAGS:-} "$root/tests/retro_bench/bench.c" \
  ${LDFLAGS:-} ${LDLIBS:-} -o "$out"
printf 'Built (NOT RUN): %s\n' "$out"
