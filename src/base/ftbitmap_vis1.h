/****************************************************************************
 *
 * ftbitmap_vis1.h
 *
 *   SPARC VIS1 64-bit vertical bitmap emboldening (internal only).
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_VIS1_H_
#define FTBITMAP_VIS1_H_

#if !defined( __sparc__ ) || !defined( __GNUC__ )
#error "FT_CONFIG_OPTION_VIS1_BITMAP_EMBOLDEN requires GCC-compatible SPARC"
#endif

/* VIS1 FOR performs a bitwise OR on two 64-bit floating-point register
 * values.  Use only 8-byte-aligned doubleword loads/stores.  Short edges
 * and unequal source/destination alignments retain scalar behavior.
 *
 * Caller supplies exactly one bitmap row.  No access crosses that row.
 */
static void
ft_bitmap_vis1_or_row( FT_Byte*        dst,
                       const FT_Byte*  src,
                       FT_Int          pitch )
{
  FT_Int  i = 0;


  if ( ( (FT_ULong)dst & 7UL ) == ( (FT_ULong)src & 7UL ) )
  {
    for ( ; i < pitch && ( (FT_ULong)( dst + i ) & 7UL ); i++ )
      dst[i] |= src[i];

    for ( ; pitch - i >= 8; i += 8 )
    {
      FT_Byte*        d = dst + i;
      const FT_Byte*  s = src + i;


      __asm__ volatile (
        "ldd [%0], %%f0\n\t"
        "ldd [%1], %%f2\n\t"
        "for %%f0, %%f2, %%f4\n\t"
        "std %%f4, [%0]\n\t"
        :
        : "r" ( d ), "r" ( s )
        : "memory", "f0", "f2", "f4" );
    }
  }

  for ( ; i < pitch; i++ )
    dst[i] |= src[i];
}

#endif /* FTBITMAP_VIS1_H_ */
