#!/bin/sh
# Host-side SIMD semantics regression. Uses no hardware-specific assembly.
set -eu
: "${CC:=cc}"
: "${CFLAGS:=-O2 -Wall -Wextra}"
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ft-retro-simd.XXXXXXXX") || exit 1
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"$CC" $CFLAGS tests/mmi_embolden_equivalence.c -o "$test_dir/retro-simd-model"
"$test_dir/retro-simd-model"
