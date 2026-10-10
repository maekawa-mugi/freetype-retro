/****************************************************************************
 *
 * ftgrays_retro.h
 *
 *   Optional long-span byte fill for R5900 MMI or SPARC VIS1.
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

static void
ft_gray_retro_fill_legacy( unsigned char*  dst,
                    int             coverage,
                    int             count )
{
  unsigned char  raw_pattern[31];
  unsigned char* pattern;
  unsigned char* p = dst;
  FT_UInt        n = (FT_UInt)count;
  FT_UInt        blocks;
  FT_UInt        i;


  if ( count < FT_GRAY_RETRO_MIN_SPAN )
  {
    FT_MEM_SET( dst, coverage, count );
    return;
  }

  pattern = (unsigned char*)( ( (FT_ULong)raw_pattern + 15UL ) &
                              ~(FT_ULong)15 );
  for ( i = 0; i < 16; ++i )
    pattern[i] = (unsigned char)coverage;

  while ( n && ( (FT_ULong)p & 15UL ) )
  {
    *p++ = (unsigned char)coverage;
    n--;
  }

  blocks = n >> 4;
  if ( blocks )
  {
    FT_ULong  word;


    /* Keep the loaded 128-bit pattern entirely inside this asm block:
     * a 32-bit C scalar cannot represent an R5900 128-bit GPR value.
     * All SQ destinations are verified 16-byte aligned.
     */
    __asm__ volatile (
      ".set push\n\t"
      ".set noreorder\n\t"
      "lq    %0, 0(%3)\n\t"
      "1:\n\t"
      "sq    %0, 0(%1)\n\t"
      "addiu %1, %1, 16\n\t"
      "addiu %2, %2, -1\n\t"
      "bne   %2, $zero, 1b\n\t"
      "nop\n\t"
      ".set pop\n\t"
      : "=&r" ( word ), "+r" ( p ), "+r" ( blocks )
      : "r" ( pattern )
      : "memory" );
    n &= 15U;
  }

  while ( n-- )
    *p++ = (unsigned char)coverage;
}

/* Broadcast in registers and store four quadwords per loop.  The libc
 * baseline remains in the harness; this candidate has no measured gate.
 */
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

#elif defined( FT_CONFIG_OPTION_VIS1_GRAY_SPANS )

#if !defined( __sparc__ ) || !defined( __GNUC__ )
#error "FT_CONFIG_OPTION_VIS1_GRAY_SPANS requires GCC-compatible SPARC"
#endif

static void
ft_gray_retro_fill( unsigned char*  dst,
                    int             coverage,
                    int             count )
{
  unsigned char  raw_pattern[15];
  unsigned char* pattern;
  unsigned char* p = dst;
  FT_UInt        n = (FT_UInt)count;
  FT_UInt        blocks, i;


  if ( count < FT_GRAY_RETRO_MIN_SPAN )
  {
    FT_MEM_SET( dst, coverage, count );
    return;
  }

  pattern = (unsigned char*)( ( (FT_ULong)raw_pattern + 7UL ) &
                              ~(FT_ULong)7 );
  for ( i = 0; i < 8; ++i )
    pattern[i] = (unsigned char)coverage;

  while ( n && ( (FT_ULong)p & 7UL ) )
  {
    *p++ = (unsigned char)coverage;
    n--;
  }

  blocks = n >> 3;
  if ( blocks )
  {
    /* Doubleword stores are present on VIS1-class SPARC.  As with the
     * MMI path, all stores are aligned and confined to the span.
     */
    __asm__ volatile (
      "ldd [%2], %%f0\n\t"
      "1:\n\t"
      "std %%f0, [%0]\n\t"
      "add %0, 8, %0\n\t"
      "subcc %1, 1, %1\n\t"
      "bne 1b\n\t"
      "nop\n\t"
      : "+r" ( p ), "+r" ( blocks )
      : "r" ( pattern )
      : "memory", "cc", "f0" );
    n &= 7U;
  }

  while ( n-- )
    *p++ = (unsigned char)coverage;
}

#endif /* selected backend */

#endif /* FTGRAYS_RETRO_H_ */
