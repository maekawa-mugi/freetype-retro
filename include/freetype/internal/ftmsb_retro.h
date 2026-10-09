/****************************************************************************
 *
 * ftmsb_retro.h
 *
 *   Opt-in most-significant-bit index implementations for the R5900
 *   Emotion Engine and 32-bit SPARC.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTMSB_RETRO_H_
#define FTMSB_RETRO_H_

#if defined( FT_CONFIG_OPTION_RETRO_MSB_R5900 ) && \
    defined( FT_CONFIG_OPTION_RETRO_MSB_SPARC32 )
#error "Select at most one retro FT_MSB backend"
#endif

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
 * for each 32-bit lane of the source GPR.  The desired result for a
 * positive nonzero 32-bit input is therefore 30 - PLZCW(low_lane).
 * For a word with its high bit set, the MSB index is always 31.
 *
 * The ordinary FreeType FT_MSB fallback returns 0 for zero.  Check
 * zero explicitly because PLZCW(0) returns 31, not an MSB index.
 * The source and output use the same low 32-bit word irrespective of
 * the sign extension in the surrounding R5900 64-bit GPR.
 */
static FT_Int
ft_msb_retro_r5900( FT_UInt32  value )
{
  FT_UInt32  count;


  if ( !value )
    return 0;

  if ( value & 0x80000000UL )
    return 31;

  __asm__ volatile (
    ".set push\n\t"
    ".set noreorder\n\t"
    "plzcw %0, %1\n\t"
    "nop\n\t"
    ".set pop\n\t"
    : "=&r" ( count )
    : "r" ( value ) );

  return (FT_Int)( 30U - count );
}

#define FT_RETRO_MSB_FUNC  ft_msb_retro_r5900


#elif defined( FT_CONFIG_OPTION_RETRO_MSB_SPARC32 )

#if !defined( __sparc__ ) || !defined( __GNUC__ ) || \
    defined( __arch64__ ) || \
    ( defined( __sparc_v9__ ) && defined( __LP64__ ) )
#error "SPARC FT_MSB requires a 32-bit GCC-compatible SPARC ABI"
#endif

/*
 * For SPARC32, first measure whether the compiler already expands
 * __builtin_clz efficiently.  This optional alternative uses only
 * ordinary 32-bit integer operations and an indexed 32-byte lookup:
 *
 *    spread highest set bit to all lower positions;
 *    multiply by a De Bruijn sequence modulo 2^32;
 *    use high five bits as index.
 *
 * Table[0] = 0 matches FreeType's scalar fallback on zero, without
 * ever calling __builtin_clz(0) (undefined).
 * The multiplication is UNSIGNED and intentionally wraps to 32 bits.
 */
static FT_Int
ft_msb_retro_sparc32( FT_UInt32  value )
{
  static const FT_Byte  table[32] =
  {
     0,  9,  1, 10, 13, 21,  2, 29,
    11, 14, 16, 18, 22, 25,  3, 30,
     8, 12, 20, 28, 15, 17, 24,  7,
    19, 27, 23,  6, 26,  5,  4, 31
  };


  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;

  return table[( (FT_UInt32)( value * 0x07C4ACDDU ) ) >> 27];
}

#define FT_RETRO_MSB_FUNC  ft_msb_retro_sparc32


#else
#error "No retro FT_MSB backend selected"
#endif

#endif /* FTMSB_RETRO_H_ */
