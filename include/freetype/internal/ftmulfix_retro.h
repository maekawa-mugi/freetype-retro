/****************************************************************************
 *
 * ftmulfix_retro.h
 *
 *   Optional signed 32x32->64 multiply for PS2 R5900.
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
 *
 * PMULTW leaves the full signed 64-bit product of the low word lane in
 * rd (and HI/LO).  The asm is not volatile and has no nops, so the
 * compiler may schedule other work in the multiply latency.  Only lane 0
 * is consumed; lane 2 sees don't-care register bits.  The result matches
 * FT_MulFix_64 exactly, including negative ties and INT32_MIN.
 */
#if FT_SIZEOF_LONG != 4
#error "Retro FT_MulFix R5900 backend requires a 32-bit FT_Long ABI"
#endif

#if !defined( __mips__ ) || !defined( __GNUC__ )
#error "R5900 FT_MulFix requires GCC-compatible MIPS32 assembly"
#endif

static inline FT_Int32
ft_mulfix_retro_pmultw( FT_Int32  a,
                        FT_Int32  b )
{
  long long  p;


  __asm__ ( "pmultw  %0, %1, %2"
            : "=r" ( p )
            : "r" ( a ), "r" ( b )
            : "hi", "lo" );

  return (FT_Int32)( ( p + 0x8000 + ( p >> 63 ) ) >> 16 );
}

#define FT_RETRO_MULFIX_FUNC  ft_mulfix_retro_pmultw

#endif /* FTMULFIX_RETRO_H_ */
