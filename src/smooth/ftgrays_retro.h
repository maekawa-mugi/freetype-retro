/****************************************************************************
 *
 * ftgrays_retro.h
 *
 *   Optional long-span byte fill for PS2 R5900 MMI.
 *   Included internally by ftgrays.c, not a public API.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTGRAYS_RETRO_H_
#define FTGRAYS_RETRO_H_

/* A glyph's short spans stay on the original seven-byte switch/memset
 * paths.  Prepare a SIMD broadcast only for relatively long runs.
 * This threshold is an initial heuristic, not a measured optimum.
 */
#define FT_GRAY_RETRO_MIN_SPAN  64

#if defined( FT_CONFIG_OPTION_MMI_GRAY_SPANS )

#if !defined( __mips__ ) || !defined( __GNUC__ )
#error "FT_CONFIG_OPTION_MMI_GRAY_SPANS requires R5900/GCC MMI support"
#endif

/* Broadcast in registers and store four quadwords per loop. */
static void
ft_gray_retro_fill( unsigned char* dst, int coverage, int count )
{
  FT_UInt n;
  if ( count < FT_GRAY_RETRO_MIN_SPAN )
  {
    FT_MEM_SET( dst, coverage, count );
    return;
  }
  n = (FT_UInt)count;
  while ( n && ( (FT_ULong)dst & 15UL ) )
  {
    *dst++ = (unsigned char)coverage;
    n--;
  }
  if ( n >= 64 )
  {
    FT_ULong pattern, upper;
    FT_UInt blocks = n / 64;
    FT_UInt byteword = (FT_UInt)(unsigned char)coverage * 0x01010101U;
    __asm__ volatile (
      ".set push\n\t" ".set noreorder\n\t"
      "dsll32 %1, %4, 0\n\t"
      "dsll32 %0, %4, 0\n\t" "dsrl32 %0, %0, 0\n\t"
      "or %0, %0, %1\n\t" "pcpyld %0, %0, %0\n\t"
      "1:\n\t"
      "sq %0, 0(%2)\n\t" "sq %0, 16(%2)\n\t"
      "sq %0, 32(%2)\n\t" "sq %0, 48(%2)\n\t"
      "addiu %3, %3, -1\n\t" "bne %3, $zero, 1b\n\t"
      "addiu %2, %2, 64\n\t" ".set pop\n\t"
      : "=&r"(pattern), "=&r"(upper), "+&r"(dst), "+&r"(blocks)
      : "r"(byteword) : "memory" );
    n %= 64;
  }
  FT_MEM_SET( dst, coverage, n );
}

#endif /* selected backend */

#endif /* FTGRAYS_RETRO_H_ */
