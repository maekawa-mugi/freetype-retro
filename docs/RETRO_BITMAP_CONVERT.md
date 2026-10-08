# Packed bitmap conversion on PS2 MMI and SPARC VIS1

This implementation adds independent, opt-in `FT_Bitmap_Convert`
acceleration for packed `FT_PIXEL_MODE_MONO` (1bpp),
`FT_PIXEL_MODE_GRAY2` (2bpp), and `FT_PIXEL_MODE_GRAY4` (4bpp).

## Build switches

* `FT_CONFIG_OPTION_MMI_BITMAP_CONVERT`: PS2 R5900, little-endian,
  GCC-compatible MMI assembler. Supports all three packed modes.
* `FT_CONFIG_OPTION_VIS1_BITMAP_CONVERT`: SPARC VIS1, big-endian,
  GCC-compatible assembler. Supports all three packed modes via FPMERGE.
* `FT_CONFIG_OPTION_NO_ASSEMBLER` overrides these switches.

The options are disabled by default. Do not enable a backend on the
other architecture. Regular FreeType builds use the unchanged scalar
implementation.

## Implementation

### R5900 MMI

* MONO: each source byte indexes eight 0/1 output bytes in a 2KiB
  table; two 64-bit `LD` loads plus `PCPYLD` and aligned `SQ`
  write 16 converted pixels.
* GRAY2: each source byte indexes four 0..3 output bytes in a 1KiB
  table. Four `LWU` loads and two packed 64-bit intermediate words
  are merged by `PCPYLD` and stored by aligned `SQ`.
* GRAY4: eight source bytes are unpacked using `PEXTLB`, `PSRLH`,
  `PSLLH`, and `POR` into 16 output bytes with the exact
  MSB-nibble-then-LSB-nibble ordering.
* The fast path for MONO/GRAY2 requires a 16-byte-aligned
  destination row. GRAY4 additionally requires an 8-byte-aligned
  source row. Other alignments and all tails use a scalar path.

The two lookup tables consume 3KiB of read-only data and may
compete with other working data in the EE's small cache. Benchmark
before deciding whether the table method is beneficial.

### SPARC VIS1

* MONO: two packed 32-bit lookup words are interleaved into eight
  8-bit pixels with `FPMERGE`. The table takes 2KiB.
* GRAY2: two source bytes are split into upper/lower interleave
  lanes using a 1KiB table; `FPMERGE` writes eight pixel values.
* GRAY4: four packed source bytes are split into upper/lower nibble
  lanes and `FPMERGE` creates eight grayscale bytes.
* The fast path requires an 8-byte-aligned destination row.
  Other rows and tails use the unchanged pixel order in C.

Both implementations output the original FreeType grayscale levels:
MONO=0/1, GRAY2=0..3, GRAY4=0..15, *without* scaling to 0..255.
Negative source/target pitch and per-row padding are handled by
FreeType's original caller. Only the active pixel width is written;
any uninitialized output padding remains untouched.

## Deferred testing

The batch command now includes a portable instruction/packing model:

    sh tests/run_retro_simd_models.sh

Its packed conversion model enumerates 197,376 width, alignment and
format combinations. It is not a target instruction execution test.

For the actual FreeType public API, compile
`tests/convert_freetype_compare.c` against independent scalar and SIMD
builds of the **same target architecture** and compare fingerprints:

    cc -O2 tests/convert_freetype_compare.c \
       $(pkg-config --cflags --libs freetype2) -o convert-fingerprint
    ./convert-fingerprint

The test is configured for 16,560 cases including both signs of source
and target pitch, odd widths, multiple row alignments, and source
padding. It hashes *active pixels only* because FT_Bitmap_Convert does
not initialize destination padding bytes.

Hardware compilation, instruction disassembly and benchmark results
are still outstanding. No measured speed improvement is claimed.
