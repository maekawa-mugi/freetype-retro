/****************************************************************************
 *
 * ftmsb_retro.h
 *
 *   Opt-in most-significant-bit index implementations for the R5900
 *   Emotion Engine.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTMSB_RETRO_H_
#define FTMSB_RETRO_H_

#if FT_SIZEOF_INT != 4 || FT_SIZEOF_LONG != 4
#error "Retro FT_MSB backends require a 32-bit int and long ABI"
#endif


#if defined( FT_CONFIG_OPTION_RETRO_MSB_R5900 )

#if ( !defined( __mips__ ) && !defined( __mips ) ) || \
    !defined( __GNUC__ )
#error "R5900 FT_MSB requires GCC-compatible MMI assembly"
#endif

/*
 * PLZCW returns (number of leading bits EQUAL TO THE SIGN BIT) - 1
 * for each 32-bit lane of the source GPR.  For v >> 1, a non-negative
 * 31-bit word, that equals clz( v ) for v >= 2 and 31 for v in { 0, 1 },
 * so FT_MSB( v ) = 31 - PLZCW( v >> 1 ) without any branch; it returns
 * 0 for v == 0 and v == 1, as the ordinary FreeType fallback does.
 */
static inline FT_Int
ft_msb_retro_r5900( FT_UInt32  value )
{
  FT_UInt32  count;


  __asm__ ( "plzcw %0, %1" : "=r" ( count ) : "r" ( value >> 1 ) );

  return (FT_Int)( 31U - count );
}

#define FT_RETRO_MSB_FUNC  ft_msb_retro_r5900


#else
#error "No retro FT_MSB backend selected"
#endif

#endif /* FTMSB_RETRO_H_ */
