"""Compare complete RG1 whole-library runs; ratios are descriptive only.

Repeat boots in alternating build order for production performance
decisions. These separate-binary runs are not RB1 paired observations.
"""
import csv
import statistics
import sys
from pathlib import Path

SIZES = (8, 12, 18, 32, 64, 128)


def read(path):
    meta = None
    checks, samples = {}, {}
    done = False
    for line in Path(path).read_text().splitlines():
        if not line.startswith("RG1,"):
            continue
        row = next(csv.reader([line]))
        if done:
            raise ValueError("records after DONE")
        if row[1] == "META":
            if meta is not None or len(row) != 8:
                raise ValueError("duplicate/bad META")
            meta = row[2:]
            if meta[1] not in ("normal", "mono", "lcd", "lcd-v", "overlap"):
                raise ValueError("unknown render mode")
            if not 0 <= int(meta[2]) <= 4 or any(int(v) <= 0 for v in meta[3:]):
                raise ValueError("invalid workload metadata")
        elif row[1] == "CHECK":
            if meta is None or len(row) != 4:
                raise ValueError("bad CHECK")
            size = int(row[2])
            if size not in SIZES or size in checks or len(row[3]) != 16:
                raise ValueError("duplicate/bad CHECK")
            checks[size] = int(row[3], 16)
        elif row[1] == "SAMPLE":
            if len(row) != 5:
                raise ValueError("bad SAMPLE")
            size, sample, ticks = map(int, row[2:])
            key = size, sample
            if size not in checks or sample not in range(6) or ticks <= 0 or key in samples:
                raise ValueError("duplicate/bad SAMPLE")
            samples[key] = ticks
        elif row == ["RG1", "DONE", "PASS"]:
            done = True
        else:
            raise ValueError("failed/unknown record")
    if not done or meta is None or set(checks) != set(SIZES):
        raise ValueError("incomplete correctness log")
    if set(samples) != {(size, s) for size in SIZES for s in range(6)}:
        raise ValueError("incomplete timing log")
    return meta, checks, samples


def compare(scalar, optimized):
    a, b = read(scalar), read(optimized)
    if a[0][1:] != b[0][1:] or a[1] != b[1]:
        raise ValueError("workload metadata or glyph fingerprints differ")
    return {size: statistics.median(a[2][size, s] for s in range(6)) /
                  statistics.median(b[2][size, s] for s in range(6))
            for size in SIZES}


if __name__ == "__main__":
    try:
        if len(sys.argv) != 3:
            raise ValueError("usage: compare_glyph.py scalar.log optimized.log")
        for size, ratio in compare(*sys.argv[1:]).items():
            print(f"{size:3d}px  {ratio:.3f}x  (scalar / optimized, descriptive)")
    except (ValueError, IndexError, OSError) as exc:
        print(f"BLOCKED: {exc}", file=sys.stderr)
        sys.exit(1)
