# Optional exact five-tap LCD span optimization

**This optimization is inactive unless `FT_CONFIG_OPTION_SUBPIXEL_RENDERING` is enabled.**
The upstream repository has that option commented out by default, so ordinary
LCD rendering uses the alternative LCD geometry path, not this filter.

## Opt-in switches

| Switch | Target | Action |
| --- | --- | --- |
| `FT_CONFIG_OPTION_RETRO_LCD_SPANS` | Both SPARC and EE, portable C | Precompute the five separately truncated tap contributions once per span, fold each long horizontal interior to a single constant byte increment, and turn the vertical pass into five contiguous constant-increment row operations |
| `FT_CONFIG_OPTION_MMI_LCD_SPANS` | R5900 EE MMI (GCC-compatible assembler) | Includes the common folded paths; when an increment span is at least 64 bytes, use aligned 16-byte `LQ`, wrapping `PADDB` and `SQ` updates |

`FT_CONFIG_OPTION_NO_ASSEMBLER` disables the MMI instruction block but
preserves the common portable folded optimization. The defaults leave
both options off. No change is made to the normal monochrome renderer
or to the non-subpixel LCD geometry path.

## Exactness

Each original coefficient is calculated as

    tap[k] = (coverage * weight[k] + 85) >> 8

and *rounded independently*. The five values must **not** be combined
before truncation, because that produces a different image.

Horizontal spans of length 5 or more consist of four head pixels,
`length-4` uniform interior pixels receiving the sum of all five taps,
and four tail pixels. For spans shorter than 5, the original five
per-sample updates are repeated in the exact order. The sums are
assigned back to bytes, preserving the original modulo-256 semantics.

**Do not use `PADDUB`.** It saturates to 255 instead of wrapping
to the low eight bits. The R5900 implementation uses `PADDB`.

For vertical filtering, each source span produces five independent
rows, each with a constant tap value. Processing one complete row per
tap maintains the byte results for valid FreeType bitmap pitches,
including negative pitch.

The 64-byte bulk threshold is an unmeasured heuristic. With tiny
glyphs, the common tap folding can be more valuable than SIMD setup.

## Deferred tests

From the repository root:

    sh tests/run_retro_simd_models.sh

That runs `tests/lcd_spans_model.c` alongside the existing host tests.
The LCD model uses the actual portable folded header, with 165,120
cases of lengths 0..257, 16 offsets, several filter-weight patterns,
transparent/opaque/intermediate coverage, overlapping spans, and
positive/negative pitch. This is *not* a hardware MMI instruction test.

For end-to-end verification, compile baseline and optimized FreeType
with the same `FT_CONFIG_OPTION_SUBPIXEL_RENDERING` setting and the same
font data. Link `tests/retro_glyph_hash.c` against each variant, then
compare `lcd` and `lcd-v` output hashes as well as per-glyph bitmaps.
On R5900 inspect the assembly for `PADDB` and aligned stores, and
measure real glyph rendering speed. Do not claim a speedup until measured.

**SPARC VIS1 status:** The common folded calculation is usable on VIS1;
this change does not add a separate VIS1-specific multiply instruction
sequence. Per-span coefficient precomputation removes those redundant
per-pixel multiplies first; any additional VIS1 vectorization should be
chosen only after profiling and ISA-level validation.
