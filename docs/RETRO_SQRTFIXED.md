# Division-free FT_SqrtFixed on 32-bit FreeType builds

An optional **portable integer C** implementation of
`FT_SqrtFixed(FT_UInt32)` has been added in
`src/base/ftsqrtrestro.h` and connected from `src/base/ftcalc.c`.

This targets PlayStation 2 EE R5900 and SPARC32 configurations that
have `FT_INT64` enabled. These CPUs can spend substantial time
executing 64-bit integer division, depending on the compiler.
The new implementation uses no divide and no 64-bit arithmetic in
its inner loop, though real measurements are required to establish
whether that is advantageous on either processor.

## Configuration

    #define FT_CONFIG_OPTION_RETRO_SQRT_RESTORING

The option is **off by default** and requires `FT_Long` to be 32
bits. It is independent of MMI/VIS1 and remains available when
`FT_CONFIG_OPTION_NO_ASSEMBLER` is set.

The alternative is selected **only if `FT_INT64` is defined**.
If `FT_INT64` is unavailable, FreeType's existing split
implementation remains intact, including its 25-cycle Meessen
method for `v > 0x10000`. This distinction avoids changing a
historically separate rounding implementation without dedicated
on-target equivalence checks.

## Exact algorithm

For `v` in the full unsigned 32-bit range, the function returns
the nearest integer to:

    sqrt(v * 65536)

which represents the square root of an unsigned 16.16 input,
expressed as 16.16. The algorithm computes an exact integer
square root of the logical **48-bit** quantity `v << 16`.

The 24-round binary restoring algorithm reads successive base-4
digits of the input:

- 16 input base-4 digits from the original 32-bit `v`
- 8 further zero digits implied by multiplication by 65536

At each round it subtracts `4*root+1` from the remainder if
that trial value fits, appending either a one or zero bit to the
partial root. All intermediate quantities fit in unsigned 32
bits. After 24 rounds:

    root = floor(sqrt(v * 65536))
    rem  = v * 65536 - root * root

To round to the nearest integer, the result is:

    root + (rem > root)

because the square of `root + 1/2` lies exactly
`root + 1/4` above `root^2`, and the 48-bit radicand is an
integer. No ties occur at exactly `root + 1/2`.

This mirrors the existing `FT_INT64` Babylonian recurrence,
including the `-1` adjustment of the radicand and the
rounded-up average. The alternative does not rely on floating
point approximations, platform-dependent long division, or
compiler-specific SIMD instructions.

The new code has 24 fixed integer rounds instead of up to six
Babylonian divide steps; **a speedup is not guaranteed**.
The right choice depends on instruction timing, loop overhead,
branch prediction and the compiler's runtime for 64-bit division.
It can be slower on processors with efficient division.

## Deferred model

Run this command from the repository root during the planned
batch verification, not during development:

    sh tests/run_retro_simd_models.sh

The new `tests/sqrtfixed_restoring_model.c` directly invokes
the actual `ft_sqrt_retro_restoring` helper and compares its
result to **two independent references**:

1. A transcription of the original `FT_INT64` Babylonian
   recurrence with the exact `r=(v<<16)-1` integer division.
2. A mathematical 64-bit integer binary-search square root
   with nearest-integer rounding.

It covers **1,458,915 input cases**: zero, extreme 32-bit
values, all 0..65535 inputs, powers of two and their neighbors,
393,216 samples adjacent to square-root transitions across
the full 32-bit input range, and one million deterministic
random input words.

The model runs on the host in ordinary C; it does not
execute target instructions or measure speed.

## Real FreeType API and glyph output

`tests/sqrtfixed_freetype_compare.c` calls the **actual internal
FreeType `FT_SqrtFixed` symbol**, recording a deterministic output
fingerprint over **512,307** inputs.

As the function is internal, build this program separately
against each 32-bit **static** FreeType archive and matching
internal headers. Enable `FT_INT64` in both builds and
`FT_CONFIG_OPTION_RETRO_SQRT_RESTORING` in only one:

    cc -O2 -Iinclude tests/sqrtfixed_freetype_compare.c \
       /path/to/build/libfreetype.a -lm \
       [other target-specific link dependencies] \
       -o sqrtfixed-fingerprint

Compare complete fingerprint output lines from the two builds.
Also compare `tests/retro_glyph_hash.c` outputs with the
same font data across normal, mono, LCD, LCD_V and overlap
rendering, especially for glyphs involving complex vector
geometry.

When the results agree, benchmark `FT_SqrtFixed` itself
and end-to-end font loading/rendering on EE and UltraSPARC.
Keep the option off if the 24-round integer loop is slower
than the original target compiler's division sequence.

**Status: implementation and deferred tests only. No tests,
cross-compilation, target runs or benchmarks have been executed.**
