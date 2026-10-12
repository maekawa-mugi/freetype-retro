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
#ifdef FT_CONFIG_OPTION_MMI_OVERLAP_SPANS

/* Sixteen destination bytes per quadword, four sequential correction
 * steps each.  A step maps p to  t = p + c;  t - ( t >> 8 )  and t >> 8
 * is 1 exactly when the byte addition carried.  PADDUB (saturating) and
 * PADDB (wrapping) differ in a lane precisely when it carried, so each
 * step is  wrap - carry,  the carry mask reduced to 1 by PAND with ones.
 * The correction is evaluated after EACH of the four samples, as in the
 * scalar repeat routine.
 */
#define FT_OVL_STEP                                   \
  "paddub  %4, %2, %0\n\t"                            \
  "paddb   %3, %2, %0\n\t"                            \
  "pceqb   %5, %4, %3\n\t"                            \
  "pand    %5, %5, %1\n\t"                            \
  "paddb   %3, %3, %5\n\t"                            \
  "psubb   %2, %3, %1\n\t"

static void
ft_smooth_mmi_overlap_quads( FT_Byte*  p,
                             FT_UInt   quads,
                             FT_UInt   cover )
{
  unsigned long long  cw   = (unsigned long long)cover *
                             0x0101010101010101ULL;
  unsigned long long  onew = 0x0101010101010101ULL;
  FT_ULong            c, one, p0, s0, w0, m0;


  __asm__ volatile (
    ".set push\n\t" ".set noreorder\n\t"
    "pcpyld  %0, %6, %6\n\t"
    "pcpyld  %1, %7, %7\n\t"
    "1:\n\t"
    "lq      %2, 0(%8)\n\t"
    FT_OVL_STEP FT_OVL_STEP FT_OVL_STEP FT_OVL_STEP
    "addiu   %9, %9, -1\n\t"
    "sq      %2, 0(%8)\n\t"
    "bgtz    %9, 1b\n\t"
    "addiu   %8, %8, 16\n\t"
    ".set pop\n\t"
    : "=&r" ( c ), "=&r" ( one ), "=&r" ( p0 ), "=&r" ( w0 ),
      "=&r" ( s0 ), "=&r" ( m0 ), "+r" ( cw ), "+r" ( onew ),
      "+r" ( p ), "+r" ( quads )
    :
    : "memory" );
}
#undef FT_OVL_STEP

#endif /* FT_CONFIG_OPTION_MMI_OVERLAP_SPANS */


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
  i = 0;

#ifdef FT_CONFIG_OPTION_MMI_OVERLAP_SPANS
  if ( groups >= 32 )
  {
    for ( ; i < groups && ( (FT_ULong)( dst + pixel + i ) & 15UL ); i++ )
      dst[pixel + i] = ft_smooth_retro_overlap_repeat( dst[pixel + i],
                                                       cover, 4 );
    if ( groups - i >= 16 )
    {
      FT_UInt  quads = ( groups - i ) >> 4;


      ft_smooth_mmi_overlap_quads( dst + pixel + i, quads, cover );
      i += quads << 4;
    }
  }
#endif

  for ( ; i < groups; i++ )
    dst[pixel + i] = ft_smooth_retro_overlap_repeat( dst[pixel + i],
                                                     cover, 4 );
  pixel += groups;

  length &= 3U;
  if ( length )
    dst[pixel] = ft_smooth_retro_overlap_repeat( dst[pixel],
                                                 cover, length );
}

#endif /* FTSMOOTH_RETRO_OVERLAP_H_ */
