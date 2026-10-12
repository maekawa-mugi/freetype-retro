/****************************************************************************
 *
 * ftsmooth_retro_lcd.h
 *
 *   Optional exact five-tap LCD span accumulation.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTSMOOTH_RETRO_LCD_H_
#define FTSMOOTH_RETRO_LCD_H_

/* The original direct LCD filter adds five independently truncated
 * coverage*weight contributions to each destination byte.
 * Every write is byte-sized, so the effective arithmetic is modulo 256.
 * Keep that behavior, even when adjacent spans overlap.
 *
 * We calculate the five taps once per span and replace the interior of
 * horizontal spans with a uniform modulo-256 byte increment.
 */
#ifdef FT_CONFIG_OPTION_MMI_LCD_MASK_SPANS

#include <freetype/internal/ftmmi_mask_retro.h>

/* Masked-quadword form: the first and last aligned quadwords are updated
 * with head/tail masks (bytes outside the span get +0), the interior with
 * four independent quadword adds per iteration.  No scalar edge loops, so
 * it also wins for short spans.  PADDB wraps, as the original does.
 */
static void
ft_smooth_retro_lcd_add_bytes( unsigned char*  p,
                               FT_UInt         len,
                               FT_UInt         increment )
{
  FT_Byte        raw[31];
  FT_Byte*       pat = (FT_Byte*)( ( (FT_ULong)raw + 15UL ) & ~15UL );
  FT_ULong       start, end_incl;
  FT_Byte*       q;
  FT_Byte*       qe;
  FT_UInt        quads;
  unsigned long long  w;


  if ( !len || !(FT_Byte)increment )
    return;

  w = (unsigned long long)(FT_Byte)increment * 0x0101010101010101ULL;
  ( (unsigned long long*)pat )[0] = w;
  ( (unsigned long long*)pat )[1] = w;

  start    = (FT_ULong)p;
  end_incl = start + len - 1;
  q  = (FT_Byte*)( start & ~15UL );
  qe = (FT_Byte*)( end_incl & ~15UL );

  if ( q == qe )
  {
    ft_mmi_rmw_masked( q, pat, ft_mmi_head_mask[start & 15UL],
                       ft_mmi_tail_mask[end_incl & 15UL] );
    return;
  }

  ft_mmi_rmw_masked( q, pat, ft_mmi_head_mask[start & 15UL],
                     ft_mmi_tail_mask[15] );
  q += 16;

  quads = (FT_UInt)( ( qe - q ) >> 4 );
  if ( quads >= 4 )
  {
    FT_UInt   blocks = quads >> 2;
    FT_ULong  pv, t0, t1, t2, t3;


    __asm__ volatile (
      ".set push\n\t" ".set noreorder\n\t"
      "lq     %0, 0(%7)\n\t"
      "1:\n\t"
      "lq     %1, 0(%6)\n\t"
      "lq     %2, 16(%6)\n\t"
      "lq     %3, 32(%6)\n\t"
      "lq     %4, 48(%6)\n\t"
      "paddb  %1, %1, %0\n\t"
      "paddb  %2, %2, %0\n\t"
      "sq     %1, 0(%6)\n\t"
      "paddb  %3, %3, %0\n\t"
      "sq     %2, 16(%6)\n\t"
      "paddb  %4, %4, %0\n\t"
      "addiu  %5, %5, -1\n\t"
      "sq     %3, 32(%6)\n\t"
      "addiu  %6, %6, 64\n\t"
      "bgtz   %5, 1b\n\t"
      "sq     %4, -16(%6)\n\t"
      ".set pop\n\t"
      : "=&r" ( pv ), "=&r" ( t0 ), "=&r" ( t1 ), "=&r" ( t2 ),
        "=&r" ( t3 ), "+r" ( blocks ), "+r" ( q )
      : "r" ( pat )
      : "memory" );
    quads &= 3U;
  }
  for ( ; quads; quads--, q += 16 )
    ft_mmi_rmw_masked( q, pat, ft_mmi_tail_mask[15], ft_mmi_tail_mask[15] );

  ft_mmi_rmw_masked( qe, pat, ft_mmi_head_mask[0],
                     ft_mmi_tail_mask[end_incl & 15UL] );
}

#else /* !FT_CONFIG_OPTION_MMI_LCD_MASK_SPANS */

static void
ft_smooth_retro_lcd_add_bytes( unsigned char*  p,
                               FT_UInt         len,
                               FT_UInt         increment )
{
  FT_Byte  delta = (FT_Byte)increment;
  FT_UInt  i = 0;


  if ( !delta || !len )
    return;

  for ( ; i < len; i++ )
    p[i] = (FT_Byte)( p[i] + delta );
}

#endif /* !FT_CONFIG_OPTION_MMI_LCD_MASK_SPANS */


/* dst is the first destination touched, i.e. two bytes to the
 * left of the first source sample, as in the original smooth renderer.
 * The renderer itself guarantees the four-filter-sample border.
 */
static void
ft_smooth_retro_lcd_horizontal( unsigned char*        dst,
                                FT_UInt               len,
                                FT_Byte               coverage,
                                const unsigned char*  weight )
{
  FT_UInt  tap[5];
  FT_UInt  i, k, total = 0;


  if ( !len )
    return;

  for ( k = 0; k < 5; k++ )
  {
    tap[k] = ( (FT_UInt)coverage * weight[k] + 85U ) >> 8;
    total += tap[k];
  }

  if ( len < 5 )
  {
    /* The head/tail regions overlap; use the exact original order. */
    for ( i = 0; i < len; ++i )
      for ( k = 0; k < 5; ++k )
        dst[i + k] = (FT_Byte)( dst[i + k] + tap[k] );
  }
  else
  {
    FT_UInt  head = 0;


    /* First four output pixels receive progressively more taps. */
    for ( i = 0; i < 4; ++i )
    {
      head += tap[i];
      dst[i] = (FT_Byte)( dst[i] + head );
    }

    /* All five taps reach each interior destination pixel. */
    ft_smooth_retro_lcd_add_bytes( dst + 4, len - 4, total );

    /* Four tail pixels receive progressively fewer taps. */
    for ( i = 0; i < 4; ++i )
    {
      FT_UInt  tail = 0;


      for ( k = i + 1; k < 5; ++k )
        tail += tap[k];

      dst[len + i] = (FT_Byte)( dst[len + i] + tail );
    }
  }
}


/* Each vertical tap writes a separate bitmap row, so a 2-D stencil
 * becomes five constant-increment spans.  No rounded intermediates
 * are combined, and both positive and negative pitch are supported.
 */
static void
ft_smooth_retro_lcd_vertical( unsigned char*        dst,
                              FT_UInt               len,
                              FT_Byte               coverage,
                              const unsigned char*  weight,
                              FT_Int                pitch )
{
  FT_UInt  k;


  if ( !len )
    return;

  for ( k = 0; k < 5; ++k )
  {
    FT_UInt  increment =
      ( (FT_UInt)coverage * weight[k] + 85U ) >> 8;


    ft_smooth_retro_lcd_add_bytes( dst, len, increment );

    /* Do not form an unused pointer one row beyond the final tap,
     * particularly when pitch is negative and dst is the last row.
     */
    if ( k < 4 )
      dst += pitch;
  }
}

#endif /* FTSMOOTH_RETRO_LCD_H_ */
