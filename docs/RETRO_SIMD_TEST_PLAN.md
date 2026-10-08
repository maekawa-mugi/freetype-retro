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

`FT_CONFIG_OPTION_NO_ASSEMBLER` disables these paths.  A standalone
`ftgrays.c` build remains scalar.  The 64-byte span threshold is an
unmeasured heuristic.

## Tier A: host model (architecture-independent)

From the repository root:

    sh tests/run_retro_simd_models.sh

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

## Tier C: end-to-end glyph rendering

Compile `tests/retro_glyph_hash.c` against each FreeType build, for
example using the same `pkg-config` command above with a different
source name.

Run with the exact same TTF/OTF font bytes:

    ./retro-glyph-hash /path/to/font.ttf normal
    ./retro-glyph-hash /path/to/font.ttf mono
    ./retro-glyph-hash /path/to/font.ttf lcd
    ./retro-glyph-hash /path/to/font.ttf lcd-v

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
