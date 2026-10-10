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
    """Read a complete RB1 run; fail closed on lost/duplicate suites.

    The CASE manifest is emitted ahead of any CHECK lines. It lists
    the exact expected suite names, variants and repetitions for this
    platform. This avoids a false SELECT from a truncated console
    capture that loses an entire suite (not just a SAMPLE line).
    """
    manifest = {}
    checks = defaultdict(list)
    timings = defaultdict(dict)
    meta = None
    gate_count = None
    done_count = None
    phase = "initial"
    failures = []
    observed_cases = set()

    with Path(path).open("r", encoding="utf8", errors="replace") as handle:
        for line_no, line in enumerate(handle, 1):
            if not line.startswith("RB1,"):
                continue
            try:
                row = next(csv.reader([line]))
                if len(row) < 2:
                    raise ValueError("missing RB1 operation")
                op = row[1]
                # Keep rejected suite names for BLOCKED diagnostics only;
                # they never substitute for the required CASE manifest.
                if op in ("CHECK", "SAMPLE") and len(row) > 2:
                    observed_cases.add(row[2])

                if op == "META":
                    if len(row) != 6 or phase != "initial":
                        raise ValueError("duplicate/out-of-order/bad META")
                    target, build, nsamples, hz = row[2:]
                    if not target or not build or int(nsamples) != SAMPLES or int(hz) <= 0:
                        raise ValueError("unexpected sampling metadata")
                    meta = {"target": target, "build": build,
                            "samples": int(nsamples), "hz": int(hz)}
                    phase = "manifest"

                elif op == "CASE":
                    if phase != "manifest" or len(row) < 7:
                        raise ValueError("out-of-order/bad CASE manifest")
                    _, _, case, reps, size, *names = row
                    if not case or case in manifest:
                        raise ValueError("empty/duplicate case name")
                    reps, size = int(reps), int(size)
                    if reps <= 0 or size <= 0:
                        raise ValueError("nonpositive case reps or size")
                    if any(not n for n in names) or len(set(names)) != len(names):
                        raise ValueError("empty/duplicate variant names")
                    if "scalar" not in names and "scalar_loop" not in names:
                        raise ValueError("CASE has no correctness oracle")
                    manifest[case] = {"reps": reps, "size": size,
                                      "variants": tuple(names)}

                elif op == "CHECK":
                    if phase not in ("manifest", "checks") or len(row) != 6:
                        raise ValueError("out-of-order/bad CHECK")
                    if not manifest:
                        raise ValueError("CHECK without CASE manifest")
                    _, _, case, variant, status, digest = row
                    if case not in manifest or variant not in manifest[case]["variants"]:
                        raise ValueError("CHECK not declared in CASE manifest")
                    if status not in ("PASS", "FAIL"):
                        raise ValueError("unknown CHECK result")
                    phase = "checks"
                    normalized = f"{int(digest, 16):08x}"
                    checks[(case, variant)].append((status, normalized))
                    if status != "PASS":
                        failures.append(f"{case}/{variant}: correctness FAIL")

                elif op == "GATE":
                    if phase != "checks" or len(row) != 4 or gate_count is not None:
                        raise ValueError("duplicate/out-of-order/bad GATE")
                    if row[2] != "PASS":
                        raise ValueError("global correctness gate failed")
                    gate_count = int(row[3])
                    if gate_count != len(manifest):
                        raise ValueError("GATE count does not match CASE manifest")
                    phase = "samples"

                elif op == "SAMPLE":
                    if phase != "samples" or len(row) != 9:
                        raise ValueError("out-of-order/bad SAMPLE")
                    _, _, case, variant, index, reps, ticks, hz, digest = row
                    if case not in manifest or variant not in manifest[case]["variants"]:
                        raise ValueError("SAMPLE not declared in CASE manifest")
                    index, reps, ticks, hz = map(int, (index, reps, ticks, hz))
                    if index < 0 or index >= SAMPLES or ticks <= 0:
                        raise ValueError("nonpositive/out-of-range sample")
                    if reps != manifest[case]["reps"]:
                        raise ValueError("SAMPLE repetition count differs from CASE")
                    if hz != meta["hz"]:
                        raise ValueError("timer frequency changed")
                    normalized = f"{int(digest, 16):08x}"
                    key = (case, variant)
                    if index in timings[key]:
                        raise ValueError("duplicate sample index")
                    timings[key][index] = (ticks, reps, hz, normalized)

                elif op == "DONE":
                    if phase != "samples" or len(row) != 4 or done_count is not None:
                        raise ValueError("duplicate/out-of-order/bad DONE")
                    if row[2] != "PASS":
                        raise ValueError("benchmark completion status is not PASS")
                    done_count = int(row[3])
                    if done_count != len(manifest) or done_count != gate_count:
                        raise ValueError("DONE/GATE counts differ from CASE manifest")
                    phase = "done"

                elif op == "FATAL":
                    failures.append("target fatal: " + ",".join(row[2:]))

                else:
                    raise ValueError(f"unknown RB1 operation: {op}")

            except (ValueError, IndexError) as exc:
                failures.append(f"{path}:{line_no}: {exc}")

    if meta is None:
        failures.append("missing META line")
    if not manifest:
        failures.append("missing CASE manifest (old logs need new ELF)")
    if gate_count is None or done_count is None or phase != "done":
        failures.append("missing successful GATE/DONE (partial or aborted log)")

    expected = {(case, variant)
                for case, desc in manifest.items()
                for variant in desc["variants"]}
    if set(checks) != expected:
        missing = sorted(expected - set(checks))
        extra = sorted(set(checks) - expected)
        failures.append(f"incomplete CHECK manifest: missing={missing} extra={extra}")
    if set(timings) != expected:
        missing = sorted(expected - set(timings))
        extra = sorted(set(timings) - expected)
        failures.append(f"incomplete SAMPLE manifest: missing={missing} extra={extra}")
    for key in sorted(expected):
        if len(checks.get(key, ())) != 5:
            failures.append(f"{key}: expected 5 CHECK trials")
        if set(timings.get(key, {})) != set(range(SAMPLES)):
            failures.append(f"{key}: expected exactly {SAMPLES} SAMPLE indices")
    return {"file": str(path), "meta": meta, "manifest": manifest,
            "checks": checks, "timings": timings, "failures": failures,
            "observed_cases": observed_cases}

def analyze(log, min_speedup, min_lower, max_jitter):
    cases = defaultdict(set)
    for (case, variant) in (set(log["checks"]) | set(log["timings"]) |
                            {(case, v)
                             for case, desc in log.get("manifest", {}).items()
                             for v in desc["variants"]}):
        cases[case].add(variant)
    rows = []
    for case in log.get("observed_cases", ()):
        if case not in cases:
            cases[case].add("(unverified)")
    invalid = bool(log["failures"])
    baselines = {}
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
        baselines[case] = baseline
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
            baseline_for_case = baselines.get(case)
            choices[case] = "KEEP " + (
                "BUILTIN" if baseline_for_case == "builtin" else
                "MEMSET" if baseline_for_case == "memset" else "SCALAR"
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
