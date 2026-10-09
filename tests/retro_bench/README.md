# FreeType Retro: correctness-gated A/B/C kernel benchmark

This is a standalone comparator ported **in design** from
[openssl-retro `codex/ps2-ee-mmi-test:test/ps2`](https://github.com/maekawa-mugi/openssl-retro/tree/codex/ps2-ee-mmi-test/test/ps2)
(`main.c`, `bench.c`, `build.sh`, `check-bench.py`).
It reuses the original harness' critical principles, not OpenSSL's
crypto workloads: independent full correctness first, equivalent
calls/inputs/repetitions, rotating A/B/C timing order, post-timer
digests, explicit target clock, and measured-only loops. The PS2SDK
compiler/linker recipe has been adapted from that repository.

The FreeType C harness lives here:

- `bench.c`: common deterministic workloads, scalar baselines,
  actual optional header kernels, PS2/Sparc hardware alternatives,
  independent checks, and RB1 logging.
- `kernels.h`: intentionally small standalone FreeType typedefs and
  selected **actual FreeType source-tree helper headers**. The helper
  experiment is not the same as whole-library API dispatch.
- `verdict.py`: host-side paired bootstrap and correctness gate, JSON
  and Markdown decisions.
- `test_verdict.py`: deferred synthetic PASS/FAIL/slow/incomplete
  analysis tests.
- `build-host.sh`, `build-ps2.sh`, `build-sparc.sh`: **build
  only** (no executable launched).
- `capture-and-judge.sh`: optional native-run wrapper, ONLY invoke
  it when ready to execute the tests.

## Complete-log protocol and fail-closed decisions

New EE/SPARC/host ELFs emit a **CASE manifest** immediately after the
RB1,META line, before validation starts:

    RB1,CASE,<suite>,<repetitions>,<input-size>,<scalar>,<candidate-A>[,...]

The analyzer requires exactly the declared case names and variants, five
CHECK records and six uniquely indexed SAMPLE records per variant.
Every SAMPLE must have the case's declared repetition count and the
META timer frequency. GATE and DONE must each occur once, in order,
with a suite count exactly equal to the CASE manifest. A missing
suite, a missing variant, duplicate or out-of-order records, malformed
counts, or a truncated console capture **blocks every SELECT**.

**Important:** logs captured from *older benchmark ELFs* lack CASE
records and are now deliberately rejected rather than being
mistaken for complete runs. Rebuild the ELF from the updated branch
before collecting measurements. This is an RB1 format extension,
not an attempt to reinterpret previous timing results.

KEEP BUILTIN, KEEP MEMSET and KEEP SCALAR are always chosen from
each workload's own deployed baseline; the decision must never
reuse the preceding workload's reference.

## Objective: choose a correct kernel, not an impressive number

For each suite and input size:

1. Generate deterministic random source and destination values
   outside the timing region. Five independent trial seeds run
   **full output-byte and guard-byte equality** against scalar.
   A mismatch stops all timing and produces a nonzero exit status.
2. Run six paired timed samples. Two candidates rotate AB/BA;
   three rotate ABC/BCA/CAB, and four rotate ABCD/BCDA/CDAB/DABC.
   Every sample reuses the same data, number of calls and timer
   frequency. All buffer resets are inside the measured common
   workload where necessary (e.g., overlap, blend, embolden).
3. Timing covers only the workload loop. Output digest and output
   logging happen AFTER timer stop; a digest mismatch aborts further
   timing on the target and the host parser independently rejects it.
4. On the host, compute the *paired* scalar/candidate duration ratios,
   sample median, 90% deterministic bootstrap confidence interval and
   median absolute deviation (MAD). Do not pool different architectures
   or multiple executables as if they were paired.
5. The comparator knows the **actual baseline already used by FreeType**:
for `msb-*` on GCC, it is `__builtin_clz`, and for long
`grayfill-*` spans it is `memset`. Portable bit scanning and
hand-written loops are still measured but are diagnostic-only.
The emulated `hi_lo_words` MulFix rounder is similarly a
correctness/timing control, not a deployable hardware choice.
Those controls cannot be automatically selected as a winner.

Mutating kernels restore only their active input/output region per
repetition (not the entire maximum-sized static buffer), applying
identical copy cost to all variants. The full guard region is checked
during pre-benchmark correctness validation. Timed MMI/VIS1 buffers
are explicitly 16-byte aligned; all five preflight offsets exercise
unaligned prefixes and tails as well.

A candidate is **ELIGIBLE** only when its median is at least 1.05x
   faster, the 90% lower confidence bound exceeds 1.02x, and MAD is
   no more than 12% of median candidate time. If fastest eligible
   variants have overlapping uncertainty, report **TIE**.
   Other decisions are BASELINE, REJECT-CORRECTNESS, REJECT-SLOW,
   INCONCLUSIVE or INCOMPLETE. Any bad/missing gate blocks selecting
   anything from the entire run.

These are *practical experimental criteria*, not a formal proof of
statistical significance or a guarantee of end-to-end performance.
Run PCSX2/real PS2 and real SPARC **separately**. For true threshold
selection (e.g. 64 vs 256 vs 4096 bytes), use the smallest range
where a variant consistently wins; never infer an exact switch point
from only three sizes.

The report also derives **provisional size crossover hints** from
measured consecutive winning sizes. Those are not precise automatic
thresholds: benchmark more sizes around each observed transition.

## Matrix and candidate provenance

| Suite | Scalar baseline | Alternative(s) | Available on |
| --- | --- | --- | --- |
| MSB | portable bit scan | `__builtin_clz`, actual SPARC De Bruijn; real PLZCW | host + targets |
| MulFix | signed 64-bit formula | common HI/LO word rounder (model); real MULT/SMUL | host + targets |
| DivFix | original FT_INT64 quotient formula | actual fast32 helper with scalar fallback | all |
| MulDiv (rounded and unrounded) | original FT_INT64 quotient | actual fast32 helper with scalar fallback | all |
| SqrtFixed | FreeType FT_INT64 Babylonian | actual restoring sqrt helper | all |
| LCD 5-tap | five scalar per-sample adds | folded portable C, real R5900 PADDB | host / EE |
| BGRA blend | original divisions by 255 | exact255 integer, 1280-byte LUT | all |
| BGRA→GRAY | original weighted squares | 3072-byte channel-square LUT | all |
| MONO embolden | original right-to-left shifts | 512-byte lookup | all |
| 4x overlap | original per-sample correction | grouped reads/writes | all |
| GRAY long span | scalar loop | libc memset, real target ASM store | all / EE / SPARC |
| Vertical bitmap OR | scalar | portable loop, real target ASM OR | all / EE / SPARC |
| MONO/GRAY2/GRAY4 convert | scalar per-pixel expansion | real MMI or VIS1 helper | EE / SPARC |
| GRAY8 horizontal embolden | saturating scalar (strength 1) | real MMI helper | EE |

On non-target host builds, there is no fabricated SIMD result:
`plzcw`, `mmi_paddb`, MMI/VIS1 conversion, and `target_or` are
**not listed**. Likewise, `hi_lo_words` is not presented as an
actual MMI/VIS1 hardware instruction benchmark; only
`target_hilo` is.

## PS2 video output and persistent final result screen

The PS2 benchmark is a **GS framebuffer application**, not just a
stdout console test. It includes PS2SDK `<debug.h>`, calls
`init_scr()` before verification, prints progress with
`scr_setXY()` / `scr_printf()`, and links `-ldebug`.
The display is initialized immediately when the ELF starts.

During the run, the display shows `VERIFY X/N` and `BENCH X/N`
with the current case name. On success the screen is reset into a
results board containing **18 kernel families** with a representative
(usually largest normal input-size) winning variant and
**provisional** scalar-relative speedup. The values are computed
from six median timing samples **after measurement**, never in the
timed loop. For deployed baselines, MSB uses GCC builtin and long
GRAY spans use libc memset, not an intentionally slow control.

- **Success:** the board says `RESULT: PASS`.
- **Correctness mismatch, timer failure, ABI error:** the screen
  says `RESULT: FAIL` with the failing phase and case.
- **Either outcome:** the EE calls PS2SDK `SleepThread()` to
  keep the image on screen until the emulator/console is stopped;
  the ELF deliberately does not exit normally.
- **Full objective verdict:** capture stdout `RB1` records and use
  `verdict.py`. The on-screen 1.05x threshold is a preview only,
  not the final CI/noise-based recommendation. Cold-LUT and per-size
  evidence appear in the full host report, not every screen row.

To update an existing checkout and **build the modified ELF**:

```sh
cd ~/freetype-retro
git pull --ff-only
bash tests/retro_bench/build-ps2.sh
# load build-retro-bench/retro-bench-ps2.elf in PCSX2 or on the EE
```

## Commands (for LATER batch testing)

Build-only host, with output path override:

```sh
sh tests/retro_bench/build-host.sh
# => build-retro-bench/retro-bench-host
```

When ready to test, optionally run and collect:

```sh
bash tests/retro_bench/capture-and-judge.sh \
  build-retro-bench/retro-bench-host build-retro-bench/host-01
# => host-01.log, host-01.md, host-01.json
```

For PS2SDK (same general linkfile/crt0 approach as OpenSSL):

```sh
export PS2DEV=/path/to/ps2dev
export PS2SDK=/path/to/ps2sdk
bash tests/retro_bench/build-ps2.sh
# => build-retro-bench/retro-bench-ps2.elf
```

Load the ELF into PCSX2 (or real PS2 using your own loader), capture
**complete stdout RB1 lines**, then parse on a host:

```sh
python3 tests/retro_bench/verdict.py ps2-console.log \
    --json ps2-verdict.json --markdown ps2-verdict.md
```

For UltraSPARC / 32-bit ABI:

```sh
CC=sparc-linux-gnu-gcc sh tests/retro_bench/build-sparc.sh
# Or set CC / SPARC_ARCH_FLAGS for your exact target toolchain
```

Run the binary **on SPARC hardware**, save stdout and parse it on the
host using the same `verdict.py`.

The parser can receive more than one log path, but it **does not pool
disjoint host/PCSX2/hardware runs**. Each ELF's scalar must be measured
on the same CPU at the same time as its competitors.

The optional analyzer-only test is:

```sh
python3 tests/retro_bench/test_verdict.py
```

## The second gate: full FreeType and real fonts

The standalone harness directly times *actual header helper kernels*
and proves their output matches the scalar reference **for these
synthetic inputs only**. Its loop overhead, preloaded LUTs and
synthetic lengths are deliberately not representative of every font.

For a merge/enable decision also require:

- `sh tests/run_retro_simd_models.sh` and existing focused reference
  programs (`tests/*_freetype_compare.c`, with matching configurations).
- Compare baseline-vs-opt-in real glyph hashes from
  `tests/retro_glyph_hash.c` for normal, mono, LCD, LCD_V and overlap,
  and color fonts when available.
- Measure glyph loading, hinting, rasterization and rendering on
  **each actual target** using the same fonts and toolchain flags.
- Confirm compiler disassembly uses the intended MMI/VIS1
  instructions and check memory alignment, ABI and instruction
  hazard requirements. A higher isolated kernel score does not
  override an end-to-end regression.

A final switch decision must satisfy **both** correctness and
repeatable performance, and needs separate recommendations by CPU,
input length and workload. If evidence is inconclusive, keep the
option disabled, rather than unconditionally choosing A or B.

**State:** harness and analyzer committed to a Draft PR.
Neither compilation, tests nor benchmarks have been executed here.
