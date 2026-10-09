/****************************************************************************
 *
 * ftmulfix_retro.h
 *
 *   Optional signed 32x32->64 multiply for PS2 R5900 / SPARC32.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTMULFIX_RETRO_H_
#define FTMULFIX_RETRO_H_

/* FT_MulFix rounds (signed_product + (negative ? 32767 : 32768))
 * arithmetically right by 16, then returns the low 32 result bits.
 * The two-word form reconstructs that result without a compiler
 * generated 64-bit multiply, sign shift, and final narrowing.
 *
 * Both HI and LO represent the ORIGINAL 64-bit signed product.
 * All carry arithmetic is intentionally unsigned.  In particular,
 * INT32_MIN and negative ties must behave exactly as FT_MulFix_64.
 */
static FT_Int32
ft_mulfix_retro_round_words( FT_UInt32  lo,
                              FT_Int32   hi )
{
  FT_UInt32  bias = hi < 0 ? 0x7FFFU : 0x8000U;
  FT_UInt32  new_lo = lo + bias;
  FT_UInt32  new_hi = (FT_UInt32)hi + ( new_lo < lo );


  return (FT_Int32)( ( new_hi << 16 ) | ( new_lo >> 16 ) );
}


#ifndef FT_RETRO_MULFIX_MODEL_ONLY

#if FT_SIZEOF_LONG != 4
#error "Retro FT_MulFix 32-bit backends require a 32-bit FT_Long ABI"
#endif

#if defined( FT_CONFIG_OPTION_RETRO_MULFIX_R5900 )

#if !defined( __mips__ ) || !defined( __GNUC__ )
#error "R5900 FT_MulFix requires GCC-compatible MIPS32 assembly"
#endif

static FT_Int32
ft_mulfix_retro_hw( FT_Int32  a,
                    FT_Int32  b )
{
  FT_UInt32  lo;
  FT_Int32   hi;


  /* The R5900 supports a signed MULT with a full 64-bit product in
   * HI:LO.  Read BOTH halves before rounding.  The explicit delay
   * slots are conservative: actual instruction timing must be checked
   * on EE hardware/assembler with the deferred target test.
   *
   * The HI/LO clobbers tell GCC that an integer multiply has replaced
   * those special-register values.  The result registers are early-
   * clobbered so neither can alias the input pair.
   */
  __asm__ volatile (
    ".set push\n\t"
    ".set noreorder\n\t"
    "mult  %2, %3\n\t"
    "nop\n\t"
    "nop\n\t"
    "nop\n\t"
    "nop\n\t"
    "nop\n\t"
    "mflo  %0\n\t"
    "mfhi  %1\n\t"
    ".set pop\n\t"
    : "=&r" ( lo ), "=&r" ( hi )
    : "r" ( a ), "r" ( b )
    : "hi", "lo" );

  return ft_mulfix_retro_round_words( lo, hi );
}

#elif defined( FT_CONFIG_OPTION_RETRO_MULFIX_SPARC32 )

#if !defined( __sparc__ ) || !defined( __GNUC__ ) || defined( __arch64__ ) || \
    defined( __sparc_v9__ ) && defined( __LP64__ )
#error "VIS1-class FT_MulFix requires a GCC-compatible 32-bit SPARC ABI"
#endif

static FT_Int32
ft_mulfix_retro_hw( FT_Int32  a,
                    FT_Int32  b )
{
  FT_UInt32  lo;
  FT_Int32   hi;


  /* SPARC V8 integer SMUL puts the upper signed product into %y.
   * Allow three instructions before RD %y.  This does NOT require
   * VIS1, only the integer multiplication support of UltraSPARC.
   * The hardware-only path is guarded by a separate option.
   */
  __asm__ volatile (
    "smul %2, %3, %0\n\t"
    "nop\n\t"
    "nop\n\t"
    "nop\n\t"
    "rd %%y, %1\n\t"
    : "=&r" ( lo ), "=&r" ( hi )
    : "r" ( a ), "r" ( b )
    : "y" );

  return ft_mulfix_retro_round_words( lo, hi );
}

#else

#error "Select exactly one retro FT_MulFix backend"

#endif

#endif /* !FT_RETRO_MULFIX_MODEL_ONLY */

#endif /* FTMULFIX_RETRO_H_ */
