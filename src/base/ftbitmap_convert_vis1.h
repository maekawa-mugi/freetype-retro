/****************************************************************************
 *
 * ftbitmap_convert_vis1.h
 *
 *   Opt-in SPARC VIS1 GRAY4-to-GRAY8 conversion.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_CONVERT_VIS1_H_
#define FTBITMAP_CONVERT_VIS1_H_

#if !defined( __sparc__ ) || !defined( __GNUC__ )
#error "FT_CONFIG_OPTION_VIS1_BITMAP_CONVERT requires GCC-compatible SPARC"
#endif
#if defined( __BYTE_ORDER__ ) && defined( __ORDER_BIG_ENDIAN__ ) && \
    __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__
#error "The VIS1 GRAY4 fast path currently requires big-endian SPARC"
#endif

/* Interleave four upper and lower nibbles with VIS1 FPMERGE:
 *
 *     upper = [a4,b4,c4,d4]; lower = [a0,b0,c0,d0]
 *     fpmerge(upper,lower) = [a4,a0,b4,b0,c4,c0,d4,d0]
 *
 * The doubleword destination must be aligned; source byte reads and
 * stack-resident FP staging loads are safe for every input alignment.
 * The source remains in its original order, and the output is 0..15.
 */
static void
ft_bitmap_vis1_convert_gray4_row( const FT_Byte* src,
                                  FT_Byte*       dst,
                                  FT_UInt        width )
{
  FT_UInt  x = 0;


  if ( ( (FT_ULong)dst & 7UL ) == 0 )
  {
    for ( ; width - x >= 8; x += 8 )
    {
      FT_UInt32  pixels = ( (FT_UInt32)src[x >> 1] << 24 ) |
                          ( (FT_UInt32)src[(x >> 1) + 1] << 16 ) |
                          ( (FT_UInt32)src[(x >> 1) + 2] <<  8 ) |
                            (FT_UInt32)src[(x >> 1) + 3];
      FT_UInt32  high = ( pixels >> 4 ) & 0x0F0F0F0FUL;
      FT_UInt32  low  = pixels & 0x0F0F0F0FUL;


      __asm__ volatile (
        "ld [%1], %%f0\n\t"
        "ld [%2], %%f1\n\t"
        "fpmerge %%f0, %%f1, %%f2\n\t"
        "std %%f2, [%0]\n\t"
        :
        : "r" ( dst + x ), "r" ( &high ), "r" ( &low )
        : "f0", "f1", "f2", "memory" );
    }
  }

  for ( ; x < width; ++x )
  {
    FT_Byte  byte = src[x >> 1];


    dst[x] = (FT_Byte)( x & 1 ? byte & 15U : byte >> 4 );
  }
}

#endif /* FTBITMAP_CONVERT_VIS1_H_ */
