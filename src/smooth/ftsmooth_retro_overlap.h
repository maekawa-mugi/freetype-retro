/****************************************************************************
 *
 * ftsmooth_retro_overlap.h
 *
 *   Opt-in exact grouping of 4x horizontal oversampling coverage.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTSMOOTH_RETRO_OVERLAP_H_
#define FTSMOOTH_RETRO_OVERLAP_H_

/* FreeType's exact per-sample correction, including its final 8-bit
 * write.  A source contribution of one to a byte of 255 produces 255:
 * (255+1) - ((255+1)>>8) = 255.
 */
static FT_Byte
ft_smooth_retro_overlap_add( FT_Byte  pixel,
                             FT_UInt  cover )
{
  FT_UInt  sum = (FT_UInt)pixel + cover;


  return (FT_Byte)( sum - ( sum >> 8 ) );
}


/* Group up to four repeated contributions by reading the output byte
 * once and writing it once.  The correction must be evaluated AFTER
 * EACH sample, not once on the combined sum.  For example, applying
 * cover=1 twice to pixel=255 yields 255 both times, whereas a combined
 * cover=2 would yield 0.  Exact semantics matter more than a faster
 * but non-equivalent clamped or wrapping vector addition.
 */
static FT_Byte
ft_smooth_retro_overlap_repeat( FT_Byte  pixel,
                                FT_UInt  cover,
                                FT_UInt  count )
{
  while ( count-- )
    pixel = ft_smooth_retro_overlap_add( pixel, cover );

  return pixel;
}


/* Each four adjacent samples from an FT_Span use the same coverage
 * and destination byte.  Only the subpixel index computation and the
 * destination memory traffic are grouped; the exact sample-level
 * arithmetic is retained, including the update order.
 *
 * This routine matches FreeType's (unsigned short)spans->x conversion,
 * has no architecture-specific dependencies, and leaves pitch/row
 * traversal to the existing renderer.
 */
static void
ft_smooth_retro_overlap_span( FT_Byte*  dst,
                              FT_UInt   x,
                              FT_UInt   length,
                              FT_UInt   coverage )
{
  FT_UInt  pixel  = x >> 2;
  FT_UInt  cover  = ( coverage + 8U ) >> 4;
  FT_UInt  first  = ( 4U - ( x & 3U ) ) & 3U;
  FT_UInt  groups;
  FT_UInt  i;


  if ( !length || !cover )
    return;

  if ( first > length )
    first = length;

  if ( first )
  {
    dst[pixel] = ft_smooth_retro_overlap_repeat( dst[pixel],
                                                 cover, first );
    pixel++;
    length -= first;
  }

  groups = length >> 2;
  for ( i = 0; i < groups; i++ )
  {
    dst[pixel] = ft_smooth_retro_overlap_repeat( dst[pixel],
                                                 cover, 4 );
    pixel++;
  }

  length &= 3U;
  if ( length )
    dst[pixel] = ft_smooth_retro_overlap_repeat( dst[pixel],
                                                 cover, length );
}

#endif /* FTSMOOTH_RETRO_OVERLAP_H_ */
