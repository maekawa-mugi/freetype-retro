#!/usr/bin/env python3
"""FreeType retro benchmark verdicts (ported A/B/F concept from openssl-retro).

Input is complete RB1 CSV stdout from SAME-ELF candidate comparisons.
A candidate is never selected unless correctness gates pass, six paired
samples exist, output digests match in every pair, and speed is credible.

CSV is emitted by bench.c without Python, JSON or printf in the timed
region, so PS2SDK and older SPARC systems need no Python runtime.
This program runs only on the *host* after collecting logs.
"""
import argparse
import csv
import json
import math
import random
import statistics
import sys
from collections import defaultdict
from pathlib import Path

SAMPLES = 6
BOOT = 2000

def pct(values, p):
    """Simple sample percentile with interpolation."""
    a = sorted(values)
    if not a:
        raise ValueError("empty percentile")
    x = (len(a) - 1) * p
    i = int(x)
    j = min(i + 1, len(a) - 1)
    return a[i] + (a[j] - a[i]) * (x - i)

def confidence(pairs, seed):
    """Paired bootstrap CI, resampling AB observations by sample index."""
    rng = random.Random(seed)
    values = []
    for _ in range(BOOT):
        sample = [pairs[rng.randrange(len(pairs))] for __ in pairs]
        values.append(statistics.median(sample))
    return pct(values, 0.05), pct(values, 0.95)

def median_abs_dev(v):
    m = statistics.median(v)
    return statistics.median(abs(x - m) for x in v)

def parse_file(path):
    checks = defaultdict(list)
    timings = defaultdict(dict)
    meta = None
    gate = False
    done = False
    failures = []
    with Path(path).open("r", encoding="utf8", errors="replace") as handle:
        for line_no, line in enumerate(handle, 1):
            if not line.startswith("RB1,"):
                continue
            try:
                row = next(csv.reader([line]))
                op = row[1]
                if op == "META":
                    if len(row) != 6 or meta is not None:
                        raise ValueError("duplicate/bad META")
                    target, build, nsamples, hz = row[2:]
                    if int(nsamples) != SAMPLES or int(hz) <= 0:
                        raise ValueError("unexpected sampling metadata")
                    meta = {"target": target, "build": build,
                            "samples": int(nsamples), "hz": int(hz)}
                elif op == "CHECK":
                    if len(row) != 6:
                        raise ValueError("bad CHECK")
                    _, _, case, variant, status, digest = row
                    if status not in ("PASS", "FAIL"):
                        raise ValueError("unknown CHECK result")
                    int(digest, 16)
                    checks[(case, variant)].append((status, digest))
                    if status != "PASS":
                        failures.append(f"{case}/{variant}: correctness FAIL")
                elif op == "SAMPLE":
                    if len(row) != 9:
                        raise ValueError("bad SAMPLE")
                    _, _, case, variant, index, reps, ticks, hz, digest = row
                    index, reps, ticks, hz = map(int, (index, reps, ticks, hz))
                    if index < 0 or index >= SAMPLES or reps <= 0 or ticks <= 0:
                        raise ValueError("nonpositive or out-of-range sample")
                    if meta and hz != meta["hz"]:
                        raise ValueError("timer frequency changed")
                    int(digest, 16)
                    key = case, variant
                    if index in timings[key]:
                        raise ValueError("duplicate sample")
                    timings[key][index] = (ticks, reps, hz, digest)
                elif op == "GATE":
                    gate = row[2] == "PASS"
                    if not gate:
                        failures.append("global correctness gate failed")
                elif op == "DONE":
                    done = row[2] == "PASS"
                    if not done:
                        failures.append("benchmark terminated without PASS")
                elif op == "FATAL":
                    failures.append("target fatal: " + ",".join(row[2:]))
            except (ValueError, IndexError) as exc:
                failures.append(f"{path}:{line_no}: {exc}")
    if meta is None:
        failures.append("missing META line")
    if not gate or not done:
        failures.append("missing successful GATE/DONE (partial or aborted log)")
    return {"file": str(path), "meta": meta,
            "checks": checks, "timings": timings, "failures": failures}

def analyze(log, min_speedup, min_lower, max_jitter):
    cases = defaultdict(set)
    for (case, variant) in set(log["checks"]) | set(log["timings"]):
        cases[case].add(variant)
    rows = []
    invalid = bool(log["failures"])
    for case in sorted(cases):
        # The true deployed baseline matters more than a deliberately
        # slow reference loop: FreeType defaults to builtin-clz on GCC
        # and to libc memset for sufficiently long GRAY spans.
        if case.startswith("msb-") and "builtin" in cases[case]:
            baseline = "builtin"
        elif case.startswith("grayfill-") and "memset" in cases[case]:
            baseline = "memset"
        elif "scalar" in cases[case]:
            baseline = "scalar"
        elif "scalar_loop" in cases[case]:
            baseline = "scalar_loop"
        else:
            rows.append(dict(case=case, variant="(all)",
                             verdict="INCOMPLETE", reason="no scalar reference"))
            invalid = True
            continue
        ref = log["timings"].get((case, baseline), {})
        ref_checks = log["checks"].get((case, baseline), [])
        for variant in sorted(cases[case]):
            key = case, variant
            checks = log["checks"].get(key, [])
            sample = log["timings"].get(key, {})
            if len(checks) != 5 or any(c[0] != "PASS" for c in checks):
                verdict, reason = "REJECT-CORRECTNESS", "missing/failing 5 tests"
                invalid = True
            elif len(ref_checks) != 5 or any(x[0] != "PASS" for x in ref_checks):
                verdict, reason = "INCOMPLETE", "invalid scalar oracle"
                invalid = True
            elif len(sample) != SAMPLES or len(ref) != SAMPLES:
                verdict, reason = "INCOMPLETE", "missing timing samples"
                invalid = True
            elif any(sample[i][1:3] != ref[i][1:3] or
                     sample[i][3] != ref[i][3] for i in range(SAMPLES)):
                verdict, reason = "REJECT-CORRECTNESS", "paired digest/reps/HZ mismatch"
                invalid = True
            elif any(checks[i][1] !=
                     ref_checks[i][1]
                     for i in range(5)):
                verdict, reason = "REJECT-CORRECTNESS", "cross-variant check digest mismatch"
                invalid = True
            else:
                ratios = [ref[i][0] / sample[i][0] for i in range(SAMPLES)]
                speedup = statistics.median(ratios)
                lo, hi = confidence(ratios, sum(map(ord, case + variant)))
                timing = [sample[i][0] for i in range(SAMPLES)]
                jitter = median_abs_dev(timing) / statistics.median(timing)
                if variant == baseline:
                    verdict, reason = "BASELINE", "-"
                elif (variant in ("hi_lo_words", "portable_loop") or
                      (case.startswith("msb-") and variant == "scalar") or
                      (case.startswith("grayfill-") and variant == "scalar_loop")):
                    verdict, reason = "CONTROL", "diagnostic, not selectable flag"
                elif jitter > max_jitter:
                    verdict, reason = "INCONCLUSIVE", "unstable timing"
                elif speedup >= min_speedup and lo > min_lower:
                    verdict, reason = "ELIGIBLE", "credible speedup"
                elif hi < 1.0:
                    verdict, reason = "REJECT-SLOW", "slower than scalar"
                elif lo <= 1.0 <= hi:
                    verdict, reason = "INCONCLUSIVE", "uncertain sign"
                else:
                    verdict, reason = "TIE", "below minimum practical gain"
                rows.append(dict(case=case, variant=variant, verdict=verdict,
                                 reason=reason, median_ticks=statistics.median(timing),
                                 speedup=round(speedup, 5), ci90_low=round(lo, 5),
                                 ci90_high=round(hi, 5), jitter=round(jitter, 4),
                                 reps=sample[0][1],
                                 target=log["meta"]["target"] if log["meta"] else "?",
                                 build=log["meta"]["build"] if log["meta"] else "?"))
                continue
            rows.append(dict(case=case, variant=variant, verdict=verdict,
                             reason=reason,
                             target=log["meta"]["target"] if log["meta"] else "?"))
    # Choose among correct and credibly faster variants. If two cannot
    # be separated by 2% in their CI estimates, leave the row tied:
    # a winner should not be declared on noise alone.
    choices = {}
    for case in sorted(cases):
        ready = [x for x in rows if x["case"] == case
                 and x["verdict"] == "ELIGIBLE"]
        ready.sort(key=lambda x: x["speedup"], reverse=True)
        if invalid:
            choices[case] = "BLOCKED (incomplete or invalid log)"
        elif not ready:
            choices[case] = "KEEP " + (
                "BUILTIN" if baseline == "builtin" else
                "MEMSET" if baseline == "memset" else "SCALAR"
            ) + " (no proven winner)"
        elif len(ready) > 1 and (
            ready[0]["ci90_low"] <= ready[1]["ci90_high"] * 1.02
        ):
            choices[case] = "TIE: " + " / ".join(x["variant"] for x in ready)
        else:
            choices[case] = "SELECT " + ready[0]["variant"]
    return rows, choices, invalid

def provisional_crossover(choices):
    """Only report supported observed size boundaries, never extrapolate.

    A suffix that has the SAME selected variant for the largest measured
    size(s) may suggest a provisional switch lower bound. No exact
    crossover can be inferred between sparse 16/256/4096-size samples.
    """
    grouped = defaultdict(list)
    for name, choice in choices.items():
        family, dash, tail = name.rpartition("-")
        if dash and tail.isdigit():
            grouped[family].append((int(tail), choice))
    bounds = {}
    for family, data in sorted(grouped.items()):
        if len(data) < 2:
            continue
        data.sort()
        last_choice = data[-1][1]
        if not last_choice.startswith("SELECT "):
            continue
        variant = last_choice[len("SELECT "):]
        first = len(data) - 1
        while first > 0 and data[first - 1][1] == last_choice:
            first -= 1
        lower = data[first][0]
        sizes = [item[0] for item in data]
        bounds[family] = {
            "candidate": variant,
            "observed_wins_at_or_above": lower,
            "measured_sizes": sizes,
            "qualification": "provisional sampled boundary only; sweep intermediate sizes",
        }
    return bounds

def markdown_report(items):
    lines = [
        "# FreeType retro: correctness-gated benchmark decisions",
        "",
        "A speedup means **scalar elapsed time / candidate elapsed time**.",
        "Six rotating-order paired observations, 90% paired bootstrap CI.",
        "Only validated candidates with a >=5% median improvement, "
        "CI lower bound >1.02, and <=12% timing MAD are eligible by default.",
        "Tied eligible implementations require another experiment.",
        "",
        "| Target | Suite/size | Decision |",
        "| --- | --- | --- |",
    ]
    for info in items:
        for case, choice in info["choices"].items():
            lines.append(f"| {info['target']} | {case} | {choice} |")
    lines += ["", "## Provisional size crossovers (measured inputs only)", "",
              "| Target | Workload | Candidate | Winning measured sizes from |",
              "| --- | --- | --- | ---: |"]
    for info in items:
        for workload, data in info["crossovers"].items():
            lines.append(f"| {info['target']} | {workload} | "
                         f"{data['candidate']} | "
                         f"{data['observed_wins_at_or_above']} |")
    lines += ["", "These are NOT exact automatic dispatcher thresholds: "
              "benchmark intermediate sizes before changing production flags.",
              ""]
    lines += ["", "## Individual alternatives", "",
              "| Target | Case | Variant | Speedup | CI 90% | Verdict |",
              "| --- | --- | --- | ---: | --- | --- |"]
    for info in items:
        for row in info["rows"]:
            speed = f"{row['speedup']:.3f}x" if "speedup" in row else "-"
            ci = f"{row['ci90_low']:.3f}–{row['ci90_high']:.3f}" \
                if "ci90_low" in row else "-"
            lines.append(f"| {info['target']} | {row['case']} | "
                         f"{row['variant']} | {speed} | {ci} | "
                         f"{row['verdict']} |")
    lines += ["", "## Interpretation", "",
              "Correctness failure is an unconditional rejection. "
              "No benchmarks are trustworthy unless GATE and DONE both pass.",
              "Results are per-input-size, not an end-to-end glyph-speed claim.",
              "Do not compare raw durations across machines, timer frequencies, "
              "or different workload builds. Compare candidate and scalar "
              "only within their same-ELF group.",
              "LUT steady-state and cold-table-setup cases are deliberately "
              "separate; the threshold decision should consider both.",
              "Full FreeType API/hash equivalence and real-font benchmarks "
              "must also pass before selecting a production flag.",
              ""]
    return "\n".join(lines)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("logs", nargs="+", help="Captured RB1 stdout log(s)")
    ap.add_argument("--json", dest="json_path", help="Write structured results")
    ap.add_argument("--markdown", help="Write a Markdown decision report")
    ap.add_argument("--min-speedup", type=float, default=1.05)
    ap.add_argument("--min-lower", type=float, default=1.02)
    ap.add_argument("--max-jitter", type=float, default=0.12)
    args = ap.parse_args()
    items = []
    any_bad = False
    for name in args.logs:
        parsed = parse_file(name)
        rows, choices, invalid = analyze(parsed, args.min_speedup,
                                          args.min_lower, args.max_jitter)
        any_bad |= invalid
        info = {"file": name, "target": parsed["meta"]["target"]
                if parsed["meta"] else "unknown",
                "build": parsed["meta"]["build"] if parsed["meta"] else "unknown",
                "failures": parsed["failures"], "rows": rows,
                "choices": choices, "crossovers": provisional_crossover(choices)}
        items.append(info)
        for error in parsed["failures"]:
            print("ERROR:", error, file=sys.stderr)
        for case, winner in choices.items():
            print(f"{info['target']:<9} {case:<25} {winner}")
    if args.json_path:
        Path(args.json_path).write_text(json.dumps(items, indent=2)+"\n",
                                        encoding="utf8")
    if args.markdown:
        Path(args.markdown).write_text(markdown_report(items),
                                        encoding="utf8")
    return 1 if any_bad else 0

if __name__ == "__main__":
    sys.exit(main())
