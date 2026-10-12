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

/* Keep original pixels in 128-bit registers INSIDE one asm region.
 * The previous block becomes the next current block on a right-to-left
 * pass.  This removes repeated tap loads and the per-block C dispatch.
 * Conservative SA spacing is retained; independent pointer/count work
 * occupies slots before the first QFSRV.  No load crosses the row.
 */
#define FT_MMI_PIPE_TAP( shift )                               \
    "nop\n\t" "nop\n\t"                                    \
    "mtsab $zero, " #shift "\n\t"                            \
    "nop\n\t" "nop\n\t" "nop\n\t"                         \
    "qfsrv %2, %0, %1\n\t"                                  \
    "paddub %3, %3, %2\n\t"
#define FT_MMI_PIPE_LOOP( extra )                              \
    __asm__ volatile (                                       \
      ".set push\n\t" ".set noreorder\n\t"                  \
      "lq %0, 0(%4)\n\t"                                   \
      "1:\n\t"                                             \
      "lq %1, -16(%4)\n\t"                                 \
      "mtsab $zero, 15\n\t"                                \
      "por %3, %0, $zero\n\t"                               \
      "addiu %5, %5, -1\n\t"                               \
      "addiu %4, %4, -16\n\t"                              \
      "qfsrv %2, %0, %1\n\t"                               \
      "paddub %3, %3, %2\n\t"                              \
      extra                                                 \
      "sq %3, 16(%4)\n\t"                                  \
      "por %0, %1, $zero\n\t"                               \
      "bne %5, $zero, 1b\n\t"                              \
      "nop\n\t"                                            \
      ".set pop\n\t"                                       \
      : "=&r" ( current ), "=&r" ( previous ),               \
        "=&r" ( shifted ), "=&r" ( sum ),                    \
        "+&r" ( cursor ), "+&r" ( blocks )                   \
      : : "memory" )

static void
ft_bitmap_mmi_gray8_embolden_small( FT_Byte* p, FT_Int pitch,
                                   FT_Int xstr )
{
  FT_Int x = pitch;
  while ( x > 0 && ( (FT_ULong)( p + x ) & 15UL ) )
  {
    FT_UInt sum;
    FT_Int i;
    x--;
    sum = p[x];
    for ( i = 1; i <= xstr && i <= x; i++ )
      sum += p[x - i];
    p[x] = (FT_Byte)( sum > 255U ? 255U : sum );
  }
  if ( x >= 32 )
  {
    FT_Int blocks = ( x - 16 ) / 16;
    FT_Byte* cursor = p + x - 16;
    FT_ULong current, previous, shifted, sum;
    x -= blocks * 16;
    switch ( xstr )
    {
    case 1: FT_MMI_PIPE_LOOP( "" ); break;
    case 2: FT_MMI_PIPE_LOOP( FT_MMI_PIPE_TAP( 14 ) ); break;
    case 3: FT_MMI_PIPE_LOOP( FT_MMI_PIPE_TAP( 14 )
                             FT_MMI_PIPE_TAP( 13 ) ); break;
    default: FT_MMI_PIPE_LOOP( FT_MMI_PIPE_TAP( 14 )
                              FT_MMI_PIPE_TAP( 13 )
                              FT_MMI_PIPE_TAP( 12 ) ); break;
    }
  }
  while ( x > 0 )
  {
    FT_UInt sum;
    FT_Int i;
    x--;
    sum = p[x];
    for ( i = 1; i <= xstr && i <= x; i++ )
      sum += p[x - i];
    p[x] = (FT_Byte)( sum > 255U ? 255U : sum );
  }
}
#undef FT_MMI_PIPE_LOOP
#undef FT_MMI_PIPE_TAP


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
        "nop\n\t"
        "nop\n\t"
        "por %0, %0, %1\n\t"
        "nop\n\t"
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
