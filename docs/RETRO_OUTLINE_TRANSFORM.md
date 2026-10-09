# Optional outline transform and translation specialization

Define the following option in `include/freetype/config/ftoption.h`:

    #define FT_CONFIG_OPTION_RETRO_OUTLINE_TRANSFORM

The option is disabled by default and is **portable C** on
PlayStation 2 Emotion Engine and SPARC32. It requires no MMI/VIS1
instructions and remains active under `FT_CONFIG_OPTION_NO_ASSEMBLER`.

## FT_Outline_Transform

The original function visits all outline points and calls
`FT_Vector_Transform` for each one, multiplying by all four matrix
coefficients. The opt-in code dispatches on the matrix:

- **Identity** (`xx=yy=65536; xy=yx=0`): do not touch points at all.
  `FT_MulFix(value,65536)` is exactly the input for 32-bit fixed
  arguments, including the negative range.
- **Diagonal** (`xy=yx=0`): calculate only
  `FT_MulFix(x,xx)` and `FT_MulFix(y,yy)`. Both omitted
  cross-products equal exactly zero under FreeType's rounding rule.
- **Generic**: inline the same four `FT_MulFix` calls that would
  otherwise occur in `FT_Vector_Transform`. Both sums and stores
  preserve their original order; the matrix is read as each point
  is visited, rather than cached outside the loop.

The existing behavior for null pointers, empty outlines and other
ordinary configurations is retained. Compilers that already inline
the baseline `FT_Vector_Transform` may see little gain for
general matrices, but avoiding whole passes for identity and two
multiplications per point for diagonal transforms are explicit
algorithmic improvements.

## FT_Outline_Translate

This option also enables specialized translation loops:

- Zero X and Y offsets: return immediately.
- X-only or Y-only offsets: modify just the affected coordinate.
- Both offsets nonzero: process two `FT_Vector` structures per
  loop iteration, with a one-point tail.

The arithmetic still uses `ADD_LONG`, as in the original
FreeType code. This is an unsigned-wrap-preserving addition,
not a saturating SIMD operation. No new pointer alignment
requirements are introduced.

## Deferred API tests

`tests/outline_transform_freetype_compare.c` checks **19,152**
outline/matrix combinations with point counts 0..255, identity,
positive/negative diagonal, shear, 90-degree rotation, zero and
random general matrices. Inside each run it compares the actual
`FT_Outline_Transform` result to individually calling
`FT_Vector_Transform` on a copy of the same points.

`tests/outline_translate_freetype_compare.c` checks **17,024**
outlines with full-range randomized 32-bit coordinates, null
or single-axis offsets, paired translations and boundary additions.
It computes the reference with FreeType-equivalent unsigned
`FT_ULong` arithmetic.

Both tests produce deterministic FNV-1a fingerprints. Compile
and link each against an otherwise identical scalar and opt-in
32-bit FreeType library, then compare complete output lines.
Follow up with the real glyph fingerprint in all render modes.

No source compilation, byte comparison, target execution or
benchmarking has been performed as part of this update.
