# Exact 32-bit FT_MulFix for PS2 EE and SPARC32

This branch introduces two mutually exclusive, **opt-in** implementations
of the signed 16.16 fixed-point multiply `FT_MulFix`.  Both are intended
for environments where a signed 64-bit multiplication in the existing
FreeType `FT_MulFix_64` function might compile to an expensive sequence.

| Switch | ISA and ABI | Full product |
| --- | --- | --- |
| `FT_CONFIG_OPTION_RETRO_MULFIX_R5900` | PlayStation 2 Emotion Engine, R5900 with 32-bit `FT_Long` | Signed `MULT`, then read `LO` and `HI` |
| `FT_CONFIG_OPTION_RETRO_MULFIX_SPARC32` | SPARC 32-bit ABI, intended for UltraSPARC VIS1 platforms | Signed `SMUL` for low 32 bits and `RD %y` for high 32 bits |

These are **integer CPU** optimizations, not MMI-packed or VIS1
floating-point vector operations.  Their main purpose is to avoid
portable 64-bit product generation on a **32-bit ABI**.  Target-dependent
code generation may already be optimal, so do not assume speedup.

The two settings are off by default and must not be enabled together.
An incompatible target compiler or non-32-bit `FT_Long` triggers a
compile-time error.  `FT_CONFIG_OPTION_NO_ASSEMBLER` restores the
original FreeType function for either requested backend.

## Exact operation

FreeType's default `FT_MulFix_64` computes:

    product = (signed 64-bit)a * b;
    rounded = product + (product < 0 ? 32767 : 32768);
    return (FT_Long)(rounded >> 16);

Both ISA backends return the original 64-bit signed product as two
32-bit words, `hi` and `lo`.  Common code applies the bias to the
**unsigned** lower word, propagates a carry to the upper word, and
extracts the 32-bit result by

    bias = hi < 0 ? 0x7fff : 0x8000;
    new_lo = lo + bias;
    new_hi = (unsigned)hi + (new_lo < lo);
    result = (new_hi << 16) | (new_lo >> 16);

This preserves exact FreeType semantics for negative values,
including negative ties and `INT32_MIN`.  It also avoids signed
overflow in the rounding operation; carry handling is unsigned.

The opt-in path is wired in two locations:

* `include/freetype/internal/ftcalc.h`: replaces the normally inlined
  `FT_MulFix(a,b)` macro used by glyph parsing, transformations and
  hinting.
* `src/base/ftcalc.c`: routes the exported `FT_MulFix` function to
  the same hardware routine, retaining unchanged fallback behavior.

All ordinary builds continue using the original implementation.

## Deferred testing

The host batch runner includes `tests/mulfix_rounding_model.c`:

    sh tests/run_retro_simd_models.sh

Its 1,037,663 cases include every integer from -65536 to 65536
multiplied by several signed boundary constants, all 35x35 extreme
pairs, and 250,000 deterministic random operand pairs.  The host model
exercises **the same two-word rounding helper used by hardware code**,
but **does not** execute `MULT/MFLO/MFHI` or `SMUL/RD %y`.

To compare the **exported public API** on the actual target, compile
`tests/mulfix_freetype_compare.c` separately against the scalar
and optimized FreeType libraries.  Its 775,517 cases produce one
deterministic FNV-1a fingerprint each:

    cc -O2 tests/mulfix_freetype_compare.c \
       $(pkg-config --cflags --libs freetype2) -o mulfix-fingerprint
    ./mulfix-fingerprint

Compare output lines for libraries built for the **same 32-bit ABI**.
Also compare `tests/retro_glyph_hash.c` output for normal, mono, LCD,
LCD_V, and overlap rendering, since internal `FT_MulFix` calls may
be compiled inline.

## Architecture-specific checks still required

1. Compile with the PS2 R5900 or SPARC32 target compiler and examine
   actual instructions, especially clobber constraints and instruction
   separation before the HI or Y-register read.
2. Check that both the exported function and internal callers really
   use the accelerated sequence.
3. Compare the test fingerprints with an otherwise identical scalar
   build and check min/max operands and signed half-way cases.
4. Measure cycles spent in `FT_MulFix` and total glyph loading/
   hinting/rendering time.  A short multiply sequence may have more
   setup cost than the compiler's best existing implementation.
5. Reject the optimization if it creates ABI, numerical or compiler
   issues, or if real performance does not improve.

**Status:** implementation and deferred tests only; neither target
assembly path has been compiled or measured as part of this update.
