# Exact fast paths for FT_MulDiv and FT_MulDiv_No_Round

The `src/base/ftmuldiv_retro.h` helper is an independent, **off by
default**, portable 32-bit arithmetic optimization for two FreeType
functions used by glyph scaling, stroking and outline geometry.

    #define FT_CONFIG_OPTION_RETRO_MULDIV_FAST32

It requires a 32-bit `FT_Long` ABI (PS2 EE or SPARC32) and works
with `FT_CONFIG_OPTION_NO_ASSEMBLER`. The implementation contains
no MMI or VIS1 instructions.

## Supported operations

For absolute magnitudes of signed operands `a,b,c`:

- `FT_MulDiv` returns `floor((a*b + floor(c/2)) / c)`, then applies
  the original sign.
- `FT_MulDiv_No_Round` returns `floor(a*b / c)`, then applies
  the original sign.

The option accelerates only cases with proven equivalence:

1. **Zero numerator** returns zero for a nonzero divisor.
2. **Cancelling factors**: `a==c` or `b==c` yields the other
   magnitude with either rounding rule.
3. **Bounded 32-bit numerators** use native unsigned 32/32 division.
   The bounds are the original FreeType thresholds
   `a+b <= 129894-(c>>17)` for rounded operations and
   `a+b <= 131071` for no-round operations.
4. **Power-of-two divisors** replace dynamic long division with an
   exact right shift after an unsigned multiplication, with the
   original rounding half-divisor included when needed.

The `FT_INT64` build uses a 64-bit unsigned product followed by a
shift, avoiding the variable 64-bit division. The emulated-64-bit
build reconstructs the full 64-bit product from four partial
16-bit cross products into two 32-bit words, adds the rounding bias
and shifts those words directly, avoiding `ft_div64by32`.

### Historical corner behavior preserved

These two existing FreeType implementations behave differently for
some extreme operands:

- The `FT_INT64` branch narrows an oversized unsigned quotient
  to 32 bits before applying its sign.
- The emulated-64-bit slow division can instead return a
  `0x7FFFFFFF` sentinel for quotients beyond 32 bits.
- The emulated-64-bit fast-check adds `a+b` using unsigned 32-bit
  arithmetic; this can itself wrap for two extreme magnitudes,
  selecting a different baseline path.

The helper preserves those distinctions: it declines optimization
when `a+b` would overflow; and in the emulated-64-bit power-of-two
case it returns the original saturation sentinel whenever the high
32 bits of the biased numerator reach/exceed the divisor.

Division by zero is never intercepted: FreeType's original sign
and sentinel behavior remains in charge.

The option optimizes both exported `FT_MulDiv` and the internal
`FT_MulDiv_No_Round` function in `src/base/ftcalc.c` without
changing either public symbol.

## Deferred host models

The batch runner compiles `tests/muldiv_fast32_model.c` **twice**:

    sh tests/run_retro_simd_models.sh

One binary defines `FT_INT64`, the other
`RETRO_MULDIV_TEST_NO_INT64`. Each compares the actual shared
helper to a separately coded reference for the matching original
FreeType semantics. Both rounding modes, signed values, zero
divisors, large quotient saturation, powers of two, and
historically overflowing `a+b` checks are represented.

There are **3,087,312 intended cases per configuration**
(6,174,624 total across both host variants), including a dense
boundary cube, 32 powers of two, and deterministic pseudo-random
operands. These case counts are *test design*, not passed tests.

## Deferred FreeType archive test

`tests/muldiv_freetype_compare.c` hashes **699,625** pairs of
results from the exported `FT_MulDiv` function and the internal
`FT_MulDiv_No_Round` function. Build this against an otherwise
identical baseline/optimized **static** FreeType archive with
matching internal headers, 32-bit ABI, and consistent
`FT_INT64` configuration. Compare the complete fingerprints.

Also run `tests/retro_glyph_hash.c` on the same fonts, since
stroke geometry and transformation call the multiplication/division
helpers from many call sites.

No host tests, cross-compilations, disassembly checks, real glyph
comparisons or hardware timing have been run in this change.
A reduction in division instruction count does **not** by itself
establish that the total rendering pipeline is faster.
