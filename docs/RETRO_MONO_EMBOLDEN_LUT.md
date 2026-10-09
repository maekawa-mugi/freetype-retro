# Adaptive packed MONO embolden lookup

A fourth optional portable integer optimization specializes the
horizontal phase of `FT_Bitmap_Embolden` when the source is packed
one-bit `FT_PIXEL_MODE_MONO`.

    #define FT_CONFIG_OPTION_RETRO_MONO_EMBOLDEN_LUT

All new options are disabled by default. This optimization uses
no MMI or VIS1 machine instructions, and remains active under
`FT_CONFIG_OPTION_NO_ASSEMBLER`. It complements the separate
R5900 GRAY8 embolden kernel and the MMI/VIS1 vertical bitmap OR.

## How the byte-level equivalence works

The original MONO horizontal loop visits bytes **from right to
left**. For byte `curr`, it ORs right-shifted copies of the
*original* byte at offsets 1 through `strength`. It also
ORs left-shifted copies of the still-unmodified immediately
preceding byte, when there is one. The strength is always clamped
to 8 pixels by FreeType.

This is the exact algebra for each output byte:

    self[curr] = curr |
      (curr >> 1) | ... | (curr >> strength)

    carry[prev] = (prev << 7) | ... |
      (prev << (8 - strength))

    output = self[curr] | carry[prev]

with byte-sized truncation as in the original C loop. For the
first byte `prev` is interpreted as zero.

The implementation creates two 256-byte tables (`self` and
`carry`) **once for the bitmap** and processes rows in descending
byte order. This preserves the original neighboring-byte ordering
and the behavior of every packed bit, including padding.

The table is used only for horizontal strengths 2..8 and bitmaps
with at least **1024 padded bytes**. All smaller bitmaps and
other pixel modes use the old code. The threshold is provisional
until real EE/SPARC glyph profiling. The tables consume 512 stack
bytes and require no heap allocation.

## Deferred models

`tests/mono_embolden_lut_model.c` invokes the **actual table
preparation and row kernel** and compares against the original
byte-by-byte nested shift loop for **81,024 intended rows**.
It covers strengths 1..8, lengths 0..1024, all 16 destination
alignments, bit-complete data and guard bytes. The host model
does not execute architecture-specific assembly.

It is registered with the existing batch command:

    sh tests/run_retro_simd_models.sh

`tests/mono_embolden_lut_freetype_compare.c` uses the public
`FT_Bitmap_Copy` and `FT_Bitmap_Embolden` APIs, covering
**5376 intended cases** across 16 bitmap widths (1..1024 bits),
eight heights, strengths 2..8, three row-padding configurations,
and both pitch signs. It fingerprints initialized output bytes
and final bitmap dimensions/stride.

Compile against otherwise identical baseline and optimized
FreeType libraries and compare all output fingerprints. Then
profile actual large 1-bit glyphs. A 512-byte table setup can
cost more than the loops for small bitmaps, which is why the
feature is adaptive and off by default.

**Status: source and regression plans committed; no compilation,
hardware verification or performance measurements performed.**
