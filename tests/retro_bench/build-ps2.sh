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
out=${1:-"$root/build-retro-bench/retro-bench-ps2.elf"}
mkdir -p "$(dirname "$out")"
"$cc" -O2 -std=c99 -march=r5900 -G0 -D_EE -DRETRO_BENCH_R5900 \
  '-DRETRO_BENCH_BUILD_ID="ps2-r5900-retro"' \
  -ffunction-sections -fdata-sections \
  -I"$PS2SDK/ee/include" -I"$PS2SDK/common/include" \
  "$root/tests/retro_bench/bench.c" \
  -B"$crt_dir/" -T"$PS2SDK/ee/startup/linkfile" \
  -L"$PS2SDK/ee/lib" -Wl,-zmax-page-size=128,--gc-sections \
  "-Wl,-Map,$out.map" -Wl,--start-group -lc -lcglue -lkernel \
  -Wl,--end-group -o "$out"
printf 'Built (NOT RUN): %s\n' "$out"
printf 'Capture RB1 stdout in PCSX2/EE; run verdict.py on a host.\n'
