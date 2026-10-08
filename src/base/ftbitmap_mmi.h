/****************************************************************************
 *
 * ftbitmap_mmi.h
 *
 *   R5900 MMI acceleration for bitmap emboldening (internal only).
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_MMI_H_
#define FTBITMAP_MMI_H_

/* This header is included only when FT_CONFIG_OPTION_MMI_BITMAP_EMBOLDEN
 * is explicitly enabled for the PlayStation 2 Emotion Engine (R5900).
 * The 128-bit GPR instructions below are not available on normal MIPS.
 */
#if !defined( __mips__ ) && !defined( __mips )
#error "FT_CONFIG_OPTION_MMI_BITMAP_EMBOLDEN requires an R5900 MIPS compiler"
#endif

/* Saturating sum of a GRAY8 pixel and its immediate left neighbour.
 * Work right-to-left so the left neighbour has not been overwritten.
 * In the vector path, both 16-byte blocks are aligned and within the row.
 * QFSRV with SA=120 forms bytes [current[-1], current[0..14]].
 */
static void
ft_bitmap_mmi_gray8_embolden_one( FT_Byte*  p,
                                  FT_Int    pitch )
{
  FT_Int  x = pitch;

  /* Finish the unaligned right edge before doing aligned stores. */
  while ( x > 0 && ( (FT_ULong)( p + x ) & 15UL ) != 0 )
  {
    FT_UInt  sum;

    x--;
    sum  = p[x];
    if ( x > 0 )
      sum += p[x - 1];
    p[x] = (FT_Byte)( sum > 255 ? 255 : sum );
  }

  /* x >= 32 guarantees that the previous aligned 16-byte block exists. */
  while ( x >= 32 )
  {
    FT_Byte*  current = p + x - 16;
    FT_Byte*  previous = current - 16;
    FT_ULong  current_value, previous_value;


    __asm__ volatile (
      "mtsab  $zero, 15\n\t"
      "lq     %0, 0(%2)\n\t"
      "lq     %1, 0(%3)\n\t"
      "nop\n\t"
      "qfsrv  %1, %0, %1\n\t"
      "paddub %0, %0, %1\n\t"
      "sq     %0, 0(%2)\n\t"
      "nop\n\t"  /* Keep MTSAB separated from the previous QFSRV. */
      : "=&r" ( current_value ), "=&r" ( previous_value )
      : "r" ( current ), "r" ( previous )
      : "memory" );

    x -= 16;
  }

  while ( x > 0 )
  {
    FT_UInt  sum;

    x--;
    sum  = p[x];
    if ( x > 0 )
      sum += p[x - 1];
    p[x] = (FT_Byte)( sum > 255 ? 255 : sum );
  }
}


/* Bitwise OR of two rows.  SIMD stores are used only when both addresses
 * share an alignment, with scalar prefix and suffix for the other bytes.
 * This also supports monochrome bitmaps, for which OR is the original rule.
 */
static void
ft_bitmap_mmi_or_row( FT_Byte*        dst,
                      const FT_Byte*  src,
                      FT_Int          pitch )
{
  FT_Int  i = 0;

  if ( ( (FT_ULong)dst & 15UL ) == ( (FT_ULong)src & 15UL ) )
  {
    while ( i < pitch && ( (FT_ULong)( dst + i ) & 15UL ) != 0 )
    {
      dst[i] |= src[i];
      i++;
    }

    for ( ; pitch - i >= 16; i += 16 )
    {
      FT_ULong  d, s;


      __asm__ volatile (
        "lq  %0, 0(%2)\n\t"
        "lq  %1, 0(%3)\n\t"
        "por %0, %0, %1\n\t"
        "sq  %0, 0(%2)\n\t"
        : "=&r" ( d ), "=&r" ( s )
        : "r" ( dst + i ), "r" ( src + i )
        : "memory" );
    }
  }

  for ( ; i < pitch; i++ )
    dst[i] |= src[i];
}

#endif /* FTBITMAP_MMI_H_ */
