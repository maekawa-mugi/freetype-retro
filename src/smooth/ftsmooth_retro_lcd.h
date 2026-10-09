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
static void
ft_smooth_retro_lcd_add_bytes( unsigned char*  p,
                               FT_UInt         len,
                               FT_UInt         increment )
{
  FT_Byte  delta = (FT_Byte)increment;
  FT_UInt  i = 0;


  if ( !delta || !len )
    return;

#if defined( FT_CONFIG_OPTION_MMI_LCD_SPANS ) && \
    !defined( FT_CONFIG_OPTION_NO_ASSEMBLER )

#if !defined( __mips__ ) || !defined( __GNUC__ )
#error "FT_CONFIG_OPTION_MMI_LCD_SPANS requires R5900 MMI and GCC"
#endif

  /* Filling the pattern costs more than bytewise C for short spans.
   * The 64-byte cutoff is a provisional threshold, not benchmarked.
   * PADDB is wrapping addition; PADDUB would saturate and change
   * FreeType's original output on overlapping filtered spans.
   */
  if ( len >= 64 )
  {
    unsigned char  raw_pattern[31];
    unsigned char* pattern =
      (unsigned char*)( ( (FT_ULong)raw_pattern + 15UL ) &
                        ~(FT_ULong)15 );


    for ( i = 0; i < 16; ++i )
      pattern[i] = delta;

    i = 0;
    while ( i < len && ( (FT_ULong)( p + i ) & 15UL ) )
    {
      p[i] = (FT_Byte)( p[i] + delta );
      i++;
    }

    while ( len - i >= 16 )
    {
      FT_ULong  pixels, amounts;


      __asm__ volatile (
        ".set push\n\t"
        ".set noreorder\n\t"
        "lq    %0, 0(%2)\n\t"
        "lq    %1, 0(%3)\n\t"
        "nop\n\t"
        "nop\n\t"
        "paddb %0, %0, %1\n\t"
        "nop\n\t"
        "sq    %0, 0(%2)\n\t"
        ".set pop\n\t"
        : "=&r" ( pixels ), "=&r" ( amounts )
        : "r" ( p + i ), "r" ( pattern )
        : "memory" );

      i += 16;
    }
  }
#endif

  for ( ; i < len; i++ )
    p[i] = (FT_Byte)( p[i] + delta );
}


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
    dst += pitch;
  }
}

#endif /* FTSMOOTH_RETRO_LCD_H_ */
