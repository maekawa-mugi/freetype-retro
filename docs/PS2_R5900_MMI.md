# PlayStation 2 (R5900 / MMI) optimizations

This tree carries a set of **opt-in** build options for the PlayStation 2
Emotion Engine.  Every option is commented out in
`include/freetype/config/ftoption.h`; with none of them defined FreeType
behaves exactly like upstream.  All options are **exact**: the output is
bit-identical to the original code.

Requirements: a GCC toolchain for the R5900 (for example the `ps2dev`
`mips64r5900el-ps2-elf-gcc`), 32-bit `long`.  Options that use MMI
instructions are ignored when `FT_CONFIG_OPTION_NO_ASSEMBLER` is defined;
the ones marked *portable C* are not.

Speedups are for the isolated kernel, measured on real PS2 hardware
against FreeType's original code path (median of six runs, spread under
2% unless noted).  They are not whole-glyph numbers.

## Fixed-point arithmetic

| Option | Replaces | Speedup |
| --- | --- | --- |
| `FT_CONFIG_OPTION_RETRO_MULFIX_R5900` | `FT_MulFix`: `PMULTW` 64-bit product, schedulable asm | 1.95x |
| `FT_CONFIG_OPTION_RETRO_MSB_R5900` | `FT_MSB`: `31 - PLZCW(v >> 1)`, branch free | 5.1x |
| `FT_CONFIG_OPTION_RETRO_SQRT_FPU` | `FT_SqrtFixed`: `sqrt.s` estimate plus an exact integer fix-up | 10.7x |
| `FT_CONFIG_OPTION_RETRO_DIVMUL_DIGITWISE` | `FT_DivFix`, `FT_MulDiv`, `FT_MulDiv_No_Round`: 64/32 division from two or three `DIVU` instead of libgcc's software 64/64 division | 1.1x - 1.6x |

`FT_DivFix` and `FT_MulDiv` are bound by the EE's divider latency, so the
gain there is modest.

## Bitmaps (`ftbitmap.c`)

| Option | Operation | Speedup |
| --- | --- | --- |
| `FT_CONFIG_OPTION_MMI_BITMAP_EMBOLDEN` | `FT_Bitmap_Embolden`: GRAY8 and the vertical row OR | 4x - 10x (GRAY8 rows of 256+ bytes), up to 23x when the glyph is mostly empty |
| `..._EMBOLDEN_MQ` | Refinement of the above: whole-row masked quadwords for short rows (up to 96 bytes) and MONO | 1.2x - 4.4x for rows under 128 bytes |
| `FT_CONFIG_OPTION_MMI_BITMAP_CONVERT` | `FT_Bitmap_Convert` of MONO, GRAY2 and GRAY4 | 2.1x - 3.9x on rows of 128+ pixels, about 1.3x below 32 |
| `..._CONVERT_EXT` | Refinement: table-free MONO (interleave tree) and GRAY2 (shift folding) for 16-byte aligned rows | MONO 3.0x - 5.4x, GRAY2 2.7x - 4.5x on rows of 128+ pixels |
| `FT_CONFIG_OPTION_MMI_BGRA_GRAY` | `FT_Bitmap_Convert` of BGRA, 16 pixels per iteration, no tables | 2.1x - 3.0x |
| `FT_CONFIG_OPTION_MMI_BLEND` | `FT_Bitmap_Blend`: four pixels per quadword; used from 512 mask pixels on | 2.0x (512 px) - 3.0x (4096 px, table setup included) |
| `FT_CONFIG_OPTION_RETRO_BLEND_EXACT255` | `FT_Bitmap_Blend` with the exact division by 255 as shift and add (*portable C*) | 1.4x |
| `FT_CONFIG_OPTION_RETRO_BLEND_LUT` | `FT_Bitmap_Blend` with a 256-entry table for large masks (*portable C*) | 1.6x - 1.8x for masks of 2048+ pixels |
| `FT_CONFIG_OPTION_RETRO_BGRA_GRAY_LUT` | `FT_Bitmap_Convert` of BGRA with square tables (*portable C*; superseded by `MMI_BGRA_GRAY`) | 1.1x |
| `FT_CONFIG_OPTION_RETRO_MONO_EMBOLDEN_LUT` | MONO emboldening with two 256-byte tables (*portable C*) | 1.7x - 2.7x |

`MMI_BLEND` implies the exact division by 255 and uses a 4 KiB table
built on the stack for each `FT_Bitmap_Blend` call; masks smaller than
512 pixels keep the scalar path.

## Rasterizers

| Option | Operation | Speedup |
| --- | --- | --- |
| `FT_CONFIG_OPTION_MMI_GRAY_SPANS` | Long gray spans filled with `SQ` quadwords | 1.1x (64 bytes) - 2.3x (4096 bytes) |
| `FT_CONFIG_OPTION_RETRO_LCD_SPANS` | Five-tap LCD filter, one wrapping byte add per span (*portable C*) | 2.5x - 7.6x |
| `FT_CONFIG_OPTION_MMI_LCD_MASK_SPANS` | MMI version with masked head and tail quadwords; also helps short spans | 2.5x (16 px) - 19x (4096 px); LCD_V 1.4x - 3.3x |
| `FT_CONFIG_OPTION_RETRO_OVERLAP_SPANS` | 4x oversampled overlap rasterizer: group the sample additions (*portable C*) | 1.2x - 1.3x |
| `FT_CONFIG_OPTION_MMI_OVERLAP_SPANS` | Sixteen-byte vector steps for spans of 128+ pixels | 1.4x (64 px) - 13x (4096 px) |

The options that were measured on hardware are listed here; code that could
not be shown to help (SPARC VIS1 backends, monochrome span fills, outline
transform shortcuts and the like) is not part of this tree.

## Typical configuration

```c
#define FT_CONFIG_OPTION_RETRO_MULFIX_R5900
#define FT_CONFIG_OPTION_RETRO_MSB_R5900
#define FT_CONFIG_OPTION_RETRO_SQRT_FPU
#define FT_CONFIG_OPTION_RETRO_DIVMUL_DIGITWISE

#define FT_CONFIG_OPTION_MMI_BITMAP_EMBOLDEN
#define FT_CONFIG_OPTION_MMI_BITMAP_EMBOLDEN_MQ
#define FT_CONFIG_OPTION_MMI_BITMAP_CONVERT
#define FT_CONFIG_OPTION_MMI_BITMAP_CONVERT_EXT
#define FT_CONFIG_OPTION_MMI_BGRA_GRAY
#define FT_CONFIG_OPTION_MMI_BLEND

#define FT_CONFIG_OPTION_MMI_GRAY_SPANS
#define FT_CONFIG_OPTION_MMI_OVERLAP_SPANS
#define FT_CONFIG_OPTION_MMI_LCD_MASK_SPANS   /* needs SUBPIXEL_RENDERING */
```

The same macros can be passed on the compiler command line instead of
editing `ftoption.h`.

## Notes

- The R5900 `PLZCW` instruction also writes a count for the upper word of
  its 64-bit register.  Inline asm that uses its result as a 32-bit value
  must canonicalize the register (`sll rd, rd, 0`), otherwise a later test
  such as `c ? ... : ...` sees the stale upper bits.  `ftdivmul_r5900.h`
  does so.
- Several kernels read and write back whole aligned quadwords that overlap
  a row; the bytes outside the row keep their values.  Rows therefore may
  be unaligned and need no padding.
