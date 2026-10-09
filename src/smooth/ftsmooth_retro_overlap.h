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

/* Original overlap raster accumulation for a single destination pixel.
 * The correction is intentionally NOT a generic saturating add: at
 * sums above 256 it wraps modulo 256 after subtracting one.
 */
static FT_Byte
ft_smooth_retro_overlap_add( FT_Byte  pixel,
                             FT_UInt  total )
{
  FT_UInt  sum = (FT_UInt)pixel + total;


  return (FT_Byte)( sum - ( sum >> 8 ) );
}


/* Each 4 neighbouring source samples uses the SAME span coverage, and
 * all four contribute to the SAME destination byte.  Each source
 * sample adds cover = (coverage+8)/16 in the original rasterizer.
 *
 * For 1..4 updates with 0<=cover<=16, at most one 256 boundary is
 * crossed, so applying the correction once to (pixel+cover*count)
 * matches applying it after EACH source sample, for all 256 possible
 * destination values.  Never use a saturating SIMD add here.
 *
 * The x coordinate is explicitly unsigned-short, matching the original
 * (unsigned short)spans->x cast.  The caller is responsible for valid
 * pixel bounds; this routine never reads past a mapped destination.
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
    dst[pixel] = ft_smooth_retro_overlap_add( dst[pixel],
                                               first * cover );
    pixel++;
    length -= first;
  }

  groups = length >> 2;
  for ( i = 0; i < groups; i++ )
  {
    dst[pixel] = ft_smooth_retro_overlap_add( dst[pixel],
                                             4U * cover );
    pixel++;
  }

  length &= 3U;
  if ( length )
    dst[pixel] = ft_smooth_retro_overlap_add( dst[pixel],
                                             length * cover );
}

#endif /* FTSMOOTH_RETRO_OVERLAP_H_ */
