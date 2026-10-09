# Opt-in 4x overlap reduction and MONO span filling

The existing raster modules have two additional opt-in kernels.  All
switches are disabled by default.  The conventional renderer is left
unchanged when the switches are off.

## 4x smooth overlap: portable exact grouping

Define `FT_CONFIG_OPTION_RETRO_OVERLAP_SPANS` to modify
`ft_smooth_overlap_spans` in `src/smooth/ftsmooth.c`.

The original callback loops over each of four horizontal subpixels
per destination pixel.  Each sample of a raster span has the same
coverage.  The new helper handles the first partial group (0..3
samples), all complete groups of four, and the final partial group.
It **loads/stores each destination byte once per group** instead of
once per sample, and reuses its precomputed destination pixel index.
Importantly, the original per-sample arithmetic is repeated in
a register for each contribution.

The original correction is:

    sum = old_pixel + (coverage + 8) / 16;
    pixel = (unsigned char)(sum - (sum >> 8));

This is NOT a generally saturating addition.  For example,
`old_pixel=255`, `cover=16` gives `(255+16-1) & 255 = 14`.
An unrestricted `PADDUB` would instead produce 255.

Repeated applications are **not** equivalent to a single application
with `cover * count`.  For example, `255 + 1` corrects back to 255
every time, while one addition of 2 produces 0.  The new helper
therefore loops over the original correction for each sample, keeping
the intermediate 8-bit store semantics in a register, and writes the
final pixel once.  The output byte range, span casting, pitch handling
and overlap update order are unchanged.

The grouping is portable C and potentially useful on both EE and
SPARC VIS1 even if a SIMD kernel is not selected.  No SIMD
speedup is assumed.

## Long monochrome raster spans

The `Vertical_Sweep_Span` path in `src/raster/ftraster.c` draws
the edge bytes with masks and fills only the *interior* bytes with
0xFF.  Short spans retain the original byte loop.

The following independent switches are available:

| Switch | Backend |
| --- | --- |
| `FT_CONFIG_OPTION_RETRO_MONO_SPANS` | Portable `FT_MEM_SET` for long interior spans |
| `FT_CONFIG_OPTION_MMI_MONO_SPANS` | PS2 EE R5900, aligned 128-bit LQ/SQ stores from an all-ones pattern |
| `FT_CONFIG_OPTION_VIS1_MONO_SPANS` | SPARC VIS1, aligned 64-bit FP LDD/STD stores from an all-ones pattern |

The long-span threshold is **64 complete interior bytes**.  This is a
provisional heuristic pending actual glyph profiling.

A SIMD store is issued only after the destination pointer is aligned
to 16 (MMI) or 8 bytes (SPARC), and only for an entirely contained
full vector.  Other bytes use scalar stores.  First and last edge
bytes retain the original masked OR operations.

`FT_CONFIG_OPTION_NO_ASSEMBLER` disables the target-specific paths.
The portable MONO switch remains available.  Standalone
`ftraster.c` compilation is unchanged, regardless of options.

## Deferred tests

From the repository root:

    sh tests/run_retro_simd_models.sh

The batch includes `tests/overlap_mono_model.c`, which uses the actual
overlap and portable MONO fill helpers and compares them against
the source algorithm, including unchanged guard bytes:

* 17,408 exhaustive repeated-correction cases: all 256 byte values,
  cover values 0..16, and 1..4 equal subpixel contributions.
* 33,408 full overlap-span cases with different alignment, positions,
  lengths, and a second overlapping raster span.
* 82,176 MONO interior-and-edge tests, including both short and
  long full-byte spans.

The native MMI/VIS1 instruction loops cannot be executed by that
portable test.  Before merging, perform target cross-compiler checks
and compare real MONO glyph rendering with the baseline.  Additionally,
exercise outlines with `FT_OUTLINE_OVERLAP` to activate the oversampled
renderer, and compare the resulting normal-mode images bit-for-bit.

No tests or measurements of the new work have been run yet.
