#!/bin/sh
# Host-side SIMD semantics regression. Uses no hardware-specific assembly.
set -eu
: "${CC:=cc}"
: "${CFLAGS:=-O2 -Wall -Wextra}"
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ft-retro-simd.XXXXXXXX") || exit 1
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"$CC" $CFLAGS tests/mmi_embolden_equivalence.c -o "$test_dir/retro-simd-model"
"$test_dir/retro-simd-model"
"$CC" $CFLAGS tests/convert_simd_model.c -o "$test_dir/retro-convert-model"
"$test_dir/retro-convert-model"
"$CC" $CFLAGS tests/blend_exact255_model.c -o "$test_dir/retro-blend-model"
"$test_dir/retro-blend-model"
"$CC" $CFLAGS tests/lcd_spans_model.c -o "$test_dir/retro-lcd-model"
"$test_dir/retro-lcd-model"
"$CC" $CFLAGS tests/overlap_mono_model.c -o "$test_dir/retro-overlap-mono-model"
"$test_dir/retro-overlap-mono-model"
