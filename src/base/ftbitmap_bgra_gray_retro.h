/****************************************************************************
 *
 * ftbitmap_bgra_gray_retro.h
 *
 *   Exact optional BGRA -> grayscale component-square lookups.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_BGRA_GRAY_RETRO_H_
#define FTBITMAP_BGRA_GRAY_RETRO_H_

/* The scalar source computes
 *
 *   l = (4731UL*B*B + 46868UL*G*G + 13937UL*R*R) >> 16;
 *   output = A ? (FT_Byte)(A - l/A) : 0;
 *
 * PRECOMPUTING EACH FULL UNSHIFTED CONTRIBUTION is essential.
 * Shifting each table entry before summation would change rounding.
 * For all B,G,R in 0..255, their sum is at most 65536*65025,
 * which fits in unsigned 32-bit arithmetic.  The 3x256 table uses
 * 3072 bytes of stack storage, with no dynamic allocation.
 */
typedef struct  FT_Retro_BGRA_Gray_Table_
{
  FT_UInt32  square[3][256];

} FT_Retro_BGRA_Gray_Table;


static void
ft_bitmap_retro_bgra_gray_prepare( FT_Retro_BGRA_Gray_Table*  table )
{
  FT_UInt  i;


  for ( i = 0; i < 256; i++ )
  {
    FT_UInt32  square = (FT_UInt32)i * i;


    table->square[0][i] = 4731UL  * square;
    table->square[1][i] = 46868UL * square;
    table->square[2][i] = 13937UL * square;
  }
}


static void
ft_bitmap_retro_bgra_gray_row( FT_Byte*                         dst,
                                const FT_Byte*                   src,
                                FT_UInt                          width,
                                const FT_Retro_BGRA_Gray_Table*   table )
{
  FT_UInt  i;


  for ( i = 0; i < width; ++i )
  {
    FT_UInt  a = src[3];


    if ( !a )
      dst[i] = 0;
    else
    {
      FT_UInt32  l = ( table->square[0][src[0]] +
                       table->square[1][src[1]] +
                       table->square[2][src[2]] ) >> 16;


      dst[i] = (FT_Byte)( a - l / a );
    }

    src += 4;
  }
}

#endif /* FTBITMAP_BGRA_GRAY_RETRO_H_ */
