# Exact 32-bit FT_DivFix fast paths and vector normalization regression

This PR adds a common, **opt-in** portable integer shortcut for the
public `FT_DivFix` API.  It is suitable for the 32-bit FreeType ABI
on the PS2 Emotion Engine and on SPARC32.

`FT_Vector_NormLen` already relies on `FT_MSB` to select its
prenormalization shift.  The patch deliberately **does not change
its Newton iterations** because the original ordering of shifts,
rounding and 32-bit wrap-sensitive arithmetic matters for exact output.

## Configuration

    #define FT_CONFIG_OPTION_RETRO_DIVFIX_FAST32

The switch is disabled by default.  It requires a **32-bit
`FT_Long`** and is ordinary integer C, not MMI or VIS1 SIMD.
`FT_CONFIG_OPTION_NO_ASSEMBLER` does not disable this portable
shortcut.

The implementation is in `src/base/ftdivfix_retro.h`. Both the
`FT_INT64` and the emulated 64-bit branches of
`src/base/ftcalc.c` dispatch through it before using the unchanged
original algorithm.

## FT_DivFix and rounding invariants

After FreeType has converted the signed input arguments into
unsigned magnitudes `a` and `b`, the original operation for
nonzero `b` is:

    floor( (65536*a + floor(b/2)) / b )

The existing sign logic is applied *after* the quotient calculation;
the new helper does not alter it.

For a power-of-two divisor `b = 2^k`:

    k <= 16: quotient = a << (16-k)
    k >  16: quotient = (a + (1 << (k-17))) >> (k-16)

These expressions are exact, including rounding at halfway points.
The shortcut declines cases where the full unsigned quotient would
not fit in 32 bits.  This is especially important because FreeType
has two different historical behaviors there:

* In the `FT_INT64` build, the final 32-bit `FT_Long` cast keeps
  the low 32 quotient bits.
* In the emulated-64-bit build, `ft_div64by32` can return a
  saturation sentinel `0x7FFFFFFF` for oversized quotients.

Those corner cases are **always delegated to the original path**.

For an ordinary divisor, the helper reuses the existing safe
numerator bound from FreeType's emulated-64-bit branch:

    a <= 65535 - (b >> 17)

When it holds, both `(a<<16)` and the added `(b>>1)` fit
in the 32-bit unsigned numerator without overflow.  The
exact result can therefore be obtained with one native 32/32
integer divide.  Other inputs retain the existing 64-bit
division path.  Division by zero also remains unchanged.

The optimized path is only a possible performance improvement.
Depending on the compiler and the amount of time spent in large
quotient cases, it can be slower.  Its speed must be measured.

## Deferred host model

From the repository root:

    sh tests/run_retro_simd_models.sh

The added `tests/divfix_fast32_model.c` compares the **actual
fast-path helper** to the original 64-bit-magnitude expression
over 1,961,233 deterministic operand pairs.  It covers:

- Positive/negative divisors and dividends, and zero denominators
- Every divisor power of two from 2^0 through 2^31
- Inputs immediately below and above 32-bit quotient overflow
- Exact sign behavior, halfway rounding, short numerator and
  high-magnitude fallback cases

It also validates the expected fast-path dispatch independently
and records how many cases use the shift, safe 32/32 divide or
unchanged fallback.

## Deferred FreeType API comparisons

Use the **same CPU, 32-bit ABI, source revision and FT_INT64
configuration** for the scalar and optimized build variants.

`tests/divfix_freetype_compare.c` links against each FreeType
variant and compares **993,225** public `FT_DivFix` cases through
an FNV-1a output fingerprint:

    cc -O2 tests/divfix_freetype_compare.c \
       $(pkg-config --cflags --libs freetype2) -o divfix-fingerprint

`tests/normlen_freetype_compare.c` compares the return length and
both normalized vector components for **300,961** inputs using the
actual internal `FT_Vector_NormLen` symbol.  Since it is internal,
**link this test against the static FreeType library** and use
matching source-tree internal headers and dependencies:

    cc -O2 -Iinclude tests/normlen_freetype_compare.c \
       /path/to/build/libfreetype.a -lm [other required link libraries] \
       -o normlen-fingerprint

The exact archive path and dependent libraries vary by toolchain.
This is a test recipe, not a claim that a particular environment
or compiler has been checked.

Compare each program's entire output line between the scalar
and optimized build.  In addition, run the glyph fingerprint
suite with normal/mono/LCD/LCD_V/overlap modes; hinting and
vector-based rendering can expose changes missed by narrow
integer tests.

**Current status:** the new optimization and test sources have
been committed. No compilation, hardware test or performance
measurement was performed.
