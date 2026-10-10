# PS2 Emotion Engine MMI bitmap emboldening

Enable at build time using `-DFT_CONFIG_OPTION_MMI_BITMAP_EMBOLDEN` in the
FreeType compilation flags, or uncomment that option in
`include/freetype/config/ftoption.h` **only when targeting the R5900 with a
GCC-compatible MMI assembler**. The default build keeps the original code.
`FT_CONFIG_OPTION_NO_ASSEMBLER` takes priority and disables the fast path.

The opt-in accelerates:

* `FT_PIXEL_MODE_GRAY`, `FT_PIXEL_MODE_LCD`, or `FT_PIXEL_MODE_LCD_V`
  with `num_grays == 256`, and 1..4 pixels of horizontal strength after
  FreeType's LCD adjustment: aligned 16-byte right-to-left PADDUB with
  QFSRV to obtain each original neighbour.  Larger strengths fall back
  to the original scalar kernel.
* Vertical OR of two bitmap rows: aligned 16-byte POR groups, for all
  supported pixel modes.

The existing scalar loop is used for other horizontal strengths, formats,
short rows, and unaligned portions.  The vectorized
horizontal kernel preserves the original-pixel contributions for all 1..4
strengths (each repeated QFSRV references an untouched original block). `LQ`/`SQ` are issued only for verified
16-byte-aligned addresses. Unlike an unaligned `LQ`/`SQ` access on R5900,
this never redirects a store to the wrong aligned address. The kernel never
reads past a scanline or writes outside its row. The fast path works with
both positive and negative bitmap pitch because the caller chooses the row
addresses before invoking these helpers.

## Long grayscale spans (second independent switch)

Define `FT_CONFIG_OPTION_MMI_GRAY_SPANS` to enable an aligned 128-bit
`SQ` fill path in `src/smooth/ftgrays.c` for uniform grayscale runs
at least 64 pixels long.  The code constructs a 16-byte repeated coverage
pattern, handles unaligned edges in C, and writes aligned 16-byte blocks
in an R5900 inline-assembly loop.  The usual short-span path stays as is.
The threshold is provisional, subject to later profiling.

The standalone `ftgrays.c` renderer ignores the optional feature.

## Validation

The pipelined row helper keeps original current/previous quadwords in
registers throughout one assembly loop. Each previous quadword becomes
the next current quadword; strengths 2..4 reuse it for every QFSRV tap.
Strength dispatch is outside the loop, and scalar edges are unchanged.
Conservative SA spacing is retained. The original helper remains a
same-ELF benchmark alternative (`mmi_legacy` versus `mmi`). This change
does not establish a hardware speedup or alter the opt-in build switch.

Run the host-side equivalence model in `tests/mmi_embolden_equivalence.c`
(via `tests/run_retro_simd_models.sh`), then compare
`FT_Bitmap_Embolden` output on an EE against the default scalar build over
varying positive/negative pitches, byte alignments, and pixel modes.
Also compare normal-mode glyph output with `tests/retro_glyph_hash.c`
using identical font data.  Benchmark on actual hardware.  No speedup
is assumed by this patch.
