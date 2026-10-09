#!/usr/bin/env bash
# Adapted from openssl-retro codex/ps2-ee-mmi-test:test/ps2/build.sh.
# Compile only; hardware execution is explicitly deferred.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
: "${PS2DEV:?Set PS2DEV to your PS2 toolchain root}"
: "${PS2SDK:?Set PS2SDK to your PS2SDK directory}"
cc=${CC:-"$PS2DEV/ee/bin/mips64r5900el-ps2-elf-gcc"}
crt_dir=${PS2EE_CRT_DIR:-"$PS2DEV/ee/mips64r5900el-ps2-elf/lib"}
if [[ ! -f "$crt_dir/crt0.o" && -f "$PS2SDK/../ee/mips64r5900el-ps2-elf/lib/crt0.o" ]]; then
  crt_dir="$PS2SDK/../ee/mips64r5900el-ps2-elf/lib"
fi
[[ -f "$crt_dir/crt0.o" && -f "$PS2SDK/ee/startup/linkfile" ]] || {
  echo "Missing crt0.o or PS2SDK startup linkfile" >&2
  exit 1
}
out=${1:-"$root/build-retro-bench/freetype_mmi.elf"}
build_jobs=${BUILD_JOBS:-${JOBS:-$(nproc)}}
(( build_jobs >= 1 )) || { echo "BUILD_JOBS must be >= 1" >&2; exit 2; }
mkdir -p "$(dirname "$out")"
build_id=$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)
make -f "$root/tests/retro_bench/Makefile.ps2" -j "$build_jobs" \
  ROOT="$root" OUT="$out" EE_CC="$cc" CRT_DIR="$crt_dir" \
  PS2SDK="$PS2SDK" BUILD_ID="$build_id" all
printf 'Built (NOT RUN): %s\n' "$out"
printf 'Capture RB1 stdout in PCSX2/EE; run verdict.py on a host.\n'
