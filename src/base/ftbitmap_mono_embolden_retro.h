/****************************************************************************
 *
 * ftbitmap_mono_embolden_retro.h
 *
 *   Optional exact byte-table MONO horizontal emboldening.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_MONO_EMBOLDEN_RETRO_H_
#define FTBITMAP_MONO_EMBOLDEN_RETRO_H_

/* The original MONO kernel processes bytes from right to left,
 * taking a snapshot of the current byte and repeatedly OR-ing
 * right-shifted bits of that snapshot and left-shifted bits of the
 * left-neighbour byte.  That neighbour is unmodified at this point.
 * We can therefore precompute both independent 256-entry terms.
 */
typedef struct FT_Retro_Mono_Embolden_Table_
{
  FT_Byte  current[256];
  FT_Byte  previous[256];

} FT_Retro_Mono_Embolden_Table;


static void
ft_bitmap_retro_mono_embolden_prepare( FT_Retro_Mono_Embolden_Table* table,
                                       FT_UInt                       strength )
{
  FT_UInt i, k;


  /* The caller clamps the monochrome strength to 1..8. */
  for ( i = 0; i < 256; i++ )
  {
    FT_UInt  current  = i;
    FT_UInt  previous = 0;


    for ( k = 1; k <= strength; k++ )
    {
      current  |= i >> k;
      previous |= i << ( 8 - k );
    }

    table->current[i]  = (FT_Byte)current;
    table->previous[i] = (FT_Byte)previous;
  }
}


static void
ft_bitmap_retro_mono_embolden_row( FT_Byte*                         p,
                                   FT_Int                           pitch,
                                   const FT_Retro_Mono_Embolden_Table* table )
{
  FT_Int  x;


  /* Descending byte order is required: p[x-1] is still the original
   * left neighbour, not a byte modified earlier in this pass.
   * The first byte has no predecessor, just like the original loop.
   */
  for ( x = pitch - 1; x >= 0; x-- )
  {
    FT_Byte  cur  = p[x];
    FT_Byte  prev = x > 0 ? p[x - 1] : 0;


    p[x] = (FT_Byte)( table->current[cur] | table->previous[prev] );
  }
}

#endif /* FTBITMAP_MONO_EMBOLDEN_RETRO_H_ */
