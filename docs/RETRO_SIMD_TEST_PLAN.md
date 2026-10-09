# Deferred SIMD regression and benchmark checklist

The optional SIMD switches are **disabled by default**.  Turn them on
only for a matching target/toolchain, and compare output to an unmodified
scalar build of the same FreeType source revision.

| Switch | Target | Optimized section |
| --- | --- | --- |
| `FT_CONFIG_OPTION_MMI_BITMAP_EMBOLDEN` | PS2 R5900 | GRAY8/LCD 1..4px horizontal emboldening and aligned 16-byte vertical OR |
| `FT_CONFIG_OPTION_MMI_GRAY_SPANS` | PS2 R5900 | 16-byte aligned stores for grayscale spans >=64px |
| `FT_CONFIG_OPTION_VIS1_BITMAP_EMBOLDEN` | SPARC VIS1 | 8-byte aligned vertical OR |
| `FT_CONFIG_OPTION_VIS1_GRAY_SPANS` | SPARC VIS1 | 8-byte aligned stores for grayscale spans >=64px |
| `FT_CONFIG_OPTION_MMI_BITMAP_CONVERT` | PS2 R5900 | MONO/GRAY2/GRAY4 16-pixel conversion |
| `FT_CONFIG_OPTION_VIS1_BITMAP_CONVERT` | SPARC VIS1 | MONO/GRAY2/GRAY4 8-pixel conversion with FPMERGE |
| `FT_CONFIG_OPTION_RETRO_BLEND_EXACT255` | Both (portable C) | Exact BGRA integer /255 + transparent pixel skip |
| `FT_CONFIG_OPTION_RETRO_BLEND_LUT` | Both (portable C) | Adaptive premultiplied color/alpha LUT for >=2048-pixel masks |
| `FT_CONFIG_OPTION_RETRO_LCD_SPANS` | Both (portable C) | Exact five-tap LCD folding into constant-increment horizontal/vertical spans |
| `FT_CONFIG_OPTION_MMI_LCD_SPANS` | PS2 R5900 | 16-byte PADDB wrapping additions for long LCD span increments, plus portable folding |
| `FT_CONFIG_OPTION_RETRO_OVERLAP_SPANS` | Both (portable C) | Group four 4x raster subpixel additions exactly |
| `FT_CONFIG_OPTION_RETRO_MONO_SPANS` | Both (portable C) | Long MONO interior bytes via memset, preserving masked edges |
| `FT_CONFIG_OPTION_MMI_MONO_SPANS` | PS2 R5900 | 128-bit aligned SQ stores for full long MONO span interiors |
| `FT_CONFIG_OPTION_VIS1_MONO_SPANS` | SPARC VIS1 | 64-bit aligned doubleword stores for full long MONO span interiors |

`FT_CONFIG_OPTION_NO_ASSEMBLER` disables architecture-specific MMI/VIS1
assembly but not the portable C optimizations.  Standalone builds of
`ftgrays.c` and `ftraster.c` remain scalar.  The 64-byte span threshold
is an unmeasured heuristic.

## Tier A: host model (architecture-independent)

From the repository root:

    sh tests/run_retro_simd_models.sh

The command also runs `tests/convert_simd_model.c` for 197,376
packed conversion format, width and address-alignment combinations.
It now also runs `tests/blend_exact255_model.c` to check all 65,026
integer-division numerators and 35,840 differential BGRA row cases.

The batch also runs `tests/lcd_spans_model.c`, exercising the common
LCD five-tap folded implementation in 165,120 length/offset/weight/coverage
and positive/negative-pitch cases, including overlapping spans.

The command additionally compiles `tests/overlap_mono_model.c`, containing
17,408 exhaustive overlap-carry identities, 33,408 full overlap-span
comparisons, and 82,176 MONO interior/edge-byte comparisons.  The
native MMI/VIS1 instruction stores are not run by this host model.

This runs deterministic byte-level models only, not actual MMI or VIS1
instructions.  It explores short/long rows, modulo-16 alignments,
1..4-pixel neighbourhoods, 8/16-byte chunk boundaries, two pitch
directions, and expected scalar fallbacks.

## Tier B: black-box FreeType equivalence

Build the scalar FreeType library and optimized FreeType library for
the **same architecture**.  Link the test program separately against
each library variant, keeping the compiler's include path and linker
search path tied to the corresponding build.  An example for systems
providing `pkg-config`:

    cc -O2 tests/embolden_freetype_compare.c \
       $(pkg-config --cflags --libs freetype2) -o embolden-fingerprint

Run the compiled program.  Compare its entire output line between
scalar and SIMD builds.  The test iterates 6,300 cases for seven bitmap
formats, 15 widths, four padding variants, strengths 0..4 horizontally
and 0/1/3 vertically, plus both signs of pitch.

These checks also cover cases where a bitmap has a wider pitch than its
pixel width.  The program hashes all allocated bitmap bytes and logical
metadata, not just the first pixel of a row.

### Packed bitmap conversion output

Compile `tests/convert_freetype_compare.c` against each library variant
and compare the output fingerprints. This additional test covers 16,560
cases across MONO, GRAY2, GRAY4, all tested alignments, both pitch signs,
and odd widths. **The test hashes only active output pixels** because
FT_Bitmap_Convert does not initialize destination row padding.

See `docs/RETRO_BITMAP_CONVERT.md` for details and memory costs.

### Exact BGRA bitmap composition output

Compile `tests/blend_freetype_compare.c` against the scalar and optimized
FreeType libraries.  Compare its result fingerprints for 11,520
BGRA blending API cases, including fully transparent, opaque and
mixed-alpha colors, small and large bitmap dimensions, and padded
positive-pitch source buffers.  The existing negative-pitch BGRA
handling has `XXX` stubs in FreeType and is out of scope.

See `docs/RETRO_BITMAP_BLEND.md` for exactness, the LUT setup cost,
and its 1280-byte stack footprint.  These are portable C fast paths,
not additional MMI/VIS1 instructions.

### LCD five-tap filtering

Build both FreeType versions with `FT_CONFIG_OPTION_SUBPIXEL_RENDERING`
enabled (it is disabled by default). Compare `lcd` and `lcd-v` output
from `tests/retro_glyph_hash.c` against the unchanged renderer. See
`docs/RETRO_LCD_SPANS.md`. Check `PADDB` instruction emission only
if the R5900 MMI option is selected; do not use saturating `PADDUB`.

## Tier C: end-to-end glyph rendering

Compile `tests/retro_glyph_hash.c` against each FreeType build, for
example using the same `pkg-config` command above with a different
source name.

Run with the exact same TTF/OTF font bytes:

    ./retro-glyph-hash /path/to/font.ttf normal
    ./retro-glyph-hash /path/to/font.ttf mono
    ./retro-glyph-hash /path/to/font.ttf lcd
    ./retro-glyph-hash /path/to/font.ttf lcd-v
    ./retro-glyph-hash /path/to/font.ttf overlap

The `overlap` mode forces `FT_OUTLINE_OVERLAP` on each outline to
exercise 4x smooth raster oversampling even if the font does not supply
this flag.  `mono` exercises the monochrome rasterizer.  In both
cases test short and very wide glyphs on the same target CPU.  See
`docs/RETRO_OVERLAP_MONO.md` for the new kernels and edge cases.

For a given font and mode, compare the output line between baseline and
optimized builds.  This covers the real gray rasterizer, including
larger glyphs with long solid spans, and the LCD rendering pipeline.

## Tier D: target ISA checks, CPU timing and memory safety

Perform these steps on the real EE or VIS1 processor (or an
instruction-accurate environment where appropriate):

1. Build with the correct R5900 or UltraSPARC instruction-set flags.
2. Disassemble `ftbitmap.c` and `ftgrays.c` objects.  Confirm
   `PADDUB`, `QFSRV`, `POR`, `SQ` on EE and `FOR` on SPARC;
   ensure no unwanted newer ISA instructions are present.
3. Compare Tier B and Tier C output with the scalar build on the same
   system.  Check all modes and pitch signs.
4. Check word/quadword alignment in the generated code, including
   allocations with padding and odd row strides.
5. Only then benchmark hot-loop cycles.  Report separately for 1, 2,
   3, 4px strengths and 8, 16, 32, 64, 128px-wide glyphs.
6. Include per-glyph total render time, and a repeat-draw workload with
   glyph caching disabled.  A speedup of one isolated routine may not
   improve total frame/render time.

**Current status:** implementation and deferred tests are committed to
the Draft PR.  The newly expanded paths are not yet confirmed by
hardware tests or measured for speed.
