#!/bin/sh
# UltraSPARC32 / VIS1 build only. Use a native or cross SPARC compiler.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
out=${1:-"$root/build-retro-bench/retro-bench-sparc32"}
mkdir -p "$(dirname -- "$out")"
build_id=$(git -C "$root" describe --always --dirty 2>/dev/null || echo unknown)
cc=${CC:-sparc-linux-gnu-gcc}
arch_flags=${SPARC_ARCH_FLAGS:-"-m32 -mcpu=ultrasparc"}
# Intentional splitting of user-configured compiler flags.
# shellcheck disable=SC2086
"$cc" -O2 -std=c99 -D_POSIX_C_SOURCE=200809L \
  -DRETRO_BENCH_SPARC32 "-DRETRO_BENCH_BUILD_ID=\"sparc32-vis1-$build_id\"" \
  $arch_flags ${CFLAGS:-} "$root/tests/retro_bench/bench.c" \
  ${LDFLAGS:-} ${LDLIBS:-} -o "$out"
printf 'Built (NOT RUN): %s\n' "$out"
