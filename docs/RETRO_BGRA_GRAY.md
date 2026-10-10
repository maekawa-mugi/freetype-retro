# Adaptive premultiplied BGRA -> GRAY conversion lookup

An optional portable C kernel has been added to
`FT_Bitmap_Convert` for `FT_PIXEL_MODE_BGRA` source bitmaps.
It is disabled by default.

    #define FT_CONFIG_OPTION_RETRO_BGRA_GRAY_LUT

The new path is chosen only for images with at least **4096 source
pixels**. Smaller images keep the existing per-pixel scalar code.
The cutoff is a provisional heuristic until EE and SPARC workloads
are benchmarked. The option also works with
`FT_CONFIG_OPTION_NO_ASSEMBLER`.

## Exact arithmetic and memory

The existing FreeType conversion for premultiplied BGRA bytes is

    a = BGRA[3];
    if (!a) return 0;
    l = (4731UL * B * B +
         46868UL * G * G +
         13937UL * R * R) >> 16;
    return (unsigned char)(a - l / a);

The opt-in implementation builds three arrays of 256 **full,
unshifted 32-bit component-square contributions**:

    blue[i]  =  4731 * i * i;
    green[i] = 46868 * i * i;
    red[i]   = 13937 * i * i;

For every output pixel it computes

    l = (blue[B] + green[G] + red[R]) >> 16;

and applies an exact reciprocal alpha division and the original byte
conversion. A 256-entry read-only table stores `ceil(65536/a)` for
nonzero alpha. For `0 <= l <= 65025`, `(l * reciprocal[a]) >> 16`
is at most one above `floor(l/a)`; subtracting `q*a > l` corrects it.
The multiplication fits in 32 bits. Alpha 255 uses the exact constant
division identity `(l + 1 + (l >> 8)) >> 8`; alpha zero still returns zero.
The original division-based LUT row is retained for harness comparison.
Shifting or rounding each lookup entry *before* the sum is
intentionally forbidden, as it would change the original image.

The maximum sum is
`(4731 + 46868 + 13937) * 255 * 255`,
which remains within unsigned 32-bit arithmetic. The lookup
uses **3072 bytes of stack memory**, plus **1024 bytes of read-only
reciprocal constants**, and allocates no heap memory.
There is no 64-bit or floating-point math, and no MMI/VIS1
instruction dependency.

## Deferred validation

The host batch `sh tests/run_retro_simd_models.sh` includes
`tests/bgra_gray_lut_model.c`. It calls the **actual** optional
table-preparation and row-conversion helpers and compares against
FreeType's original formula for **100,000 randomized rows** plus
1280 dedicated single-pixel color/alpha patterns. It covers
all byte magnitudes, transparent/opaque/intermediate alpha,
non-premultiplied RGBA inputs, row widths 0..257, misaligned
destination offsets and guard-byte preservation.

`tests/bgra_gray_freetype_compare.c` performs **3240 public API
conversions** across 18 widths, nine heights, positive/negative
source pitches, row padding and five alpha scenarios. Some images
are below and some above the 4096-pixel table threshold.
The result fingerprint hashes only initialized grayscale
pixels, not uninitialized output row padding.

Compile the public API test separately against unchanged and
opt-in FreeType libraries for the same CPU/ABI and compare the
entire fingerprint. In addition, use real color-font bitmaps,
especially large color emoji and CBDT glyphs, to benchmark
whether the 3KB table setup cost is recovered by the reduction
in channel squaring.

**This is an optional optimization, not a confirmed speedup.
No tests, compilation or benchmarking have been run yet.**
