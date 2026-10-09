# Exact BGRA bitmap composition, common to R5900 and SPARC VIS1

Two **portable integer** fast paths have been added to
`src/base/ftbitmap.c` without changing the default FreeType build.

| Opt-in setting | Behavior |
| --- | --- |
| `FT_CONFIG_OPTION_RETRO_BLEND_EXACT255` | Exact integer division-by-255, plus bypass of transparent source pixels |
| `FT_CONFIG_OPTION_RETRO_BLEND_LUT` | Implies the exact-div255 path and optionally precomputes a color/alpha LUT for large bitmaps |

These options are **not MMI or VIS1 instructions**.  Both processors can
benefit if their C compiler emits slower division and multiplication
sequences, but only cycle measurements will show whether they help.
`FT_CONFIG_OPTION_NO_ASSEMBLER` does **not** suppress these portable
C options.  All optimizations are disabled by default.

## Exact math

Every integer division in the existing inner BGRA blending loop is
of the form `(a*b)/255`, where `0 <= a,b <= 255` (a product between
0 and 65025).  Over this domain:

    floor(n/255) == (n + 1 + (n >> 8)) >> 8

This is mathematically exact, unlike approximate alpha compositing
formulas that round.  We retain the original operation ordering:

1. `fa = floor(color.alpha * coverage / 255)`
2. `fb/fg/fr = floor(color.channel * fa / 255)`
3. `inverse = 255 - fa`
4. `dst.channel = (byte)(floor(old_dst.channel * inverse / 255) + foreground.channel)`

The conversion to `FT_Byte` still happens at the same final
assignment step.  A coverage of zero is an identity operation and can
be skipped.  An entirely transparent source color can skip the row.

## Optional lookup-table strategy

For source bitmaps with at least 2048 pixels, the LUT option builds a
table indexed by the **original coverage byte**:

* `foreground[256][4]` stores 1024 bytes of already-quantized BGRA
  premultiplied source channels.
* `inverse[256]` stores 256 bytes of background opacity factors.

This uses **1280 bytes of stack** plus a setup loop of 256 entries
per blend call.  The source alpha is quantized *before* computing its
premultiplied RGB channels, so the two-stage truncation in FreeType
is preserved.  Below the threshold, the same option falls back to
the exact integer direct path without table preparation.

The EE has a small data cache: LUT setup cost, locality, and stack
pressure may outweigh the benefit for small and medium glyphs.
The 2048-pixel cutoff is a provisional heuristic, not a measured
optimum.  The LUT is especially unlikely to help tiny glyphs.

The original target allocation, row traversal, offset computation,
source-format conversion and pitch-sign handling are unchanged.
The upstream FreeType code has two `/* XXX */` branches for
negative-pitch BGRA target copying and composition; this patch does
not attempt to fix those behaviors.

## Deferred comparison tests

For a portable differential model that includes the **actual new
internal row helpers**, run:

    sh tests/run_retro_simd_models.sh

The added `tests/blend_exact255_model.c` checks all 65,026 possible
products and 35,840 row/color/coverage/alignment combinations for
both the direct row and lookup-table functions.  It does not
compile or execute MMI/VIS1 instructions.

For a black-box public API comparison, build
`tests/blend_freetype_compare.c` twice, against scalar and optimized
FreeType libraries of the **same architecture**.  Its fingerprint
covers 11,520 cases of source coverage patterns, BGRA colors,
bitmap widths/heights and source pitches:

    cc -O2 tests/blend_freetype_compare.c \
       $(pkg-config --cflags --libs freetype2) -o blend-fingerprint
    ./blend-fingerprint

Compare the entire output line from the two binaries on the same
target.  Because negative-pitch blend behavior has TODO branches in
the original FreeType source, this specific suite uses positive pitch.

No target performance result or build verification is claimed.
