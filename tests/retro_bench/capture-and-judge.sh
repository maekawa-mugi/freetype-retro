#!/usr/bin/env bash
# Run a chosen native binary and produce verdicts. This never attempts
# to execute PS2 ELF from a host; for PS2, capture console stdout manually.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
if [[ $# -lt 2 ]]; then
  echo "usage: $0 /path/to/native/bench /path/to/output-prefix" >&2
  exit 2
fi
binary=$1
prefix=$2
mkdir -p "$(dirname "$prefix")"
"$binary" > "$prefix.log" 2>&1
python3 "$root/tests/retro_bench/verdict.py" "$prefix.log" \
  --json "$prefix.json" --markdown "$prefix.md"
printf 'Results: %s.log %s.json %s.md\n' "$prefix" "$prefix" "$prefix"
