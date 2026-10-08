# PS2 Emotion Engine MMI bitmap emboldening

Enable at build time using `-DFT_CONFIG_OPTION_MMI_BITMAP_EMBOLDEN` in the
FreeType compilation flags, or uncomment that option in
`include/freetype/config/ftoption.h` **only when targeting the R5900 with a
GCC-compatible MMI assembler**. The default build keeps the original code.
`FT_CONFIG_OPTION_NO_ASSEMBLER` takes priority and disables the fast path.

The opt-in accelerates:

* `FT_PIXEL_MODE_GRAY`, `num_grays == 256`, `xStrength == 1` after rounding:
  16-byte right-to-left PADDUB with QFSRV to obtain left neighbours.
* Vertical OR of two bitmap rows: aligned 16-byte POR groups, for all
  supported pixel modes.

The existing scalar loop is used for other horizontal strengths, formats,
short rows, and unaligned portions. `LQ`/`SQ` are issued only for verified
16-byte-aligned addresses. Unlike an unaligned `LQ`/`SQ` access on R5900,
this never redirects a store to the wrong aligned address. The kernel never
reads past a scanline or writes outside its row. The fast path works with
both positive and negative bitmap pitch because the caller chooses the row
addresses before invoking these helpers.

## Validation

Run the host-side equivalence model in `tests/mmi_embolden_equivalence.c`,
and then compare `FT_Bitmap_Embolden` output on an EE against the default
scalar build over varying pitches, byte alignments, and pixel modes.
Benchmark on actual hardware. No speedup is assumed by this patch.
