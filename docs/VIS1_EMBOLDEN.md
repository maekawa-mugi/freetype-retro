# SPARC VIS1 bitmap emboldening

Enable the opt-in kernel with
`-DFT_CONFIG_OPTION_VIS1_BITMAP_EMBOLDEN` in your FreeType
compilation flags, or uncomment that macro in
`include/freetype/config/ftoption.h`.  The target must be a SPARC
implementation with VIS1 (for example, UltraSPARC) and a
GCC-compatible assembler; use an appropriate `-mcpu=` flag so the
assembler accepts VIS instructions.

This currently accelerates **only the vertical bitmap OR** step in
`FT_Bitmap_Embolden`.  The source and target row must have the same
alignment modulo 8, otherwise the original scalar loop is used.
When possible the routine first handles the alignment prefix, then
uses 8-byte FP load / VIS1 FOR / FP store, followed by a scalar tail.
No part of a pixel row is read or written beyond its bounds.
Horizontal emboldening and other FreeType rendering logic are unchanged.

The default and `FT_CONFIG_OPTION_NO_ASSEMBLER` builds remain scalar.

## Long grayscale spans (second independent switch)

Define `FT_CONFIG_OPTION_VIS1_GRAY_SPANS` to enable 64-bit aligned
SPARC doubleword stores for grayscale spans of 64 bytes or more in
`src/smooth/ftgrays.c`.  This path only needs the doubleword FP
load/store facilities of a VIS1-class SPARC, not a VIS-only instruction.
It does not change the short-span code or the standalone `ftgrays.c` build.
Its 64-byte cutoff is a heuristic pending benchmarking.

## Deferred validation

* Run `tests/run_retro_simd_models.sh` for the common host byte-model.
  This exercises the 64-bit OR grouping but cannot execute SPARC VIS1.
* Cross-compile with the desired VIS1 options enabled (and disabled for the baseline) and
  compare `FT_Bitmap_Embolden` output byte-for-byte.
* Verify positive and negative pitch and each byte alignment.
* Run `tests/retro_glyph_hash.c` with the same input font to compare
  real raster output of scalar and optimized libraries.
* Benchmark on real VIS1 hardware; no performance gain is assumed.

See also `docs/MMI_EMBOLDEN.md` for the independent R5900 backend.
