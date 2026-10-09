/****************************************************************************
 *
 * ftbitmap_blend_retro.h
 *
 *   Opt-in integer-exact premultiplied BGRA bitmap composition.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_BLEND_RETRO_H_
#define FTBITMAP_BLEND_RETRO_H_

/*
 * For unsigned n = x*y and 0 <= x,y <= 255, n <= 65025:
 *
 *     floor(n/255) == (n + 1 + (n >> 8)) >> 8
 *
 * This is an exact integer identity, not the rounded (n+128)*257
 * approximation sometimes used for alpha composition.
 * All intermediates fit in an unsigned 32-bit integer.
 */
static FT_UInt
ft_bitmap_retro_div255( FT_UInt  n )
{
  return ( n + 1U + ( n >> 8 ) ) >> 8;
}


/* A direct, no-allocation path for small glyphs and masks.  It follows
 * the existing FT_Bitmap_Blend operation ordering: first compute the
 * integer foreground alpha, then premultiply each color channel, then
 * fade each destination component and add the foreground component.
 *
 * A zero mask or zero source alpha is an identity operation.  Skipping
 * it preserves the destination bytes, including the alpha channel.
 */
static void
ft_bitmap_retro_blend_row( FT_Byte*        dst,
                           const FT_Byte*  coverage,
                           FT_UInt         width,
                           FT_Color        color )
{
  FT_UInt  i;


  if ( !color.alpha )
    return;

  for ( i = 0; i < width; ++i, dst += 4 )
  {
    FT_UInt  aa = coverage[i];
    FT_UInt  fa, fb, fg, fr, inverse;


    if ( !aa )
      continue;

    fa      = ft_bitmap_retro_div255( (FT_UInt)color.alpha * aa );
    inverse = 255U - fa;
    fb      = ft_bitmap_retro_div255( (FT_UInt)color.blue  * fa );
    fg      = ft_bitmap_retro_div255( (FT_UInt)color.green * fa );
    fr      = ft_bitmap_retro_div255( (FT_UInt)color.red   * fa );

    dst[0] = (FT_Byte)( ft_bitmap_retro_div255( dst[0] * inverse ) + fb );
    dst[1] = (FT_Byte)( ft_bitmap_retro_div255( dst[1] * inverse ) + fg );
    dst[2] = (FT_Byte)( ft_bitmap_retro_div255( dst[2] * inverse ) + fr );
    dst[3] = (FT_Byte)( ft_bitmap_retro_div255( dst[3] * inverse ) + fa );
  }
}


#ifdef FT_CONFIG_OPTION_RETRO_BLEND_LUT

/*
 * Source color and inverse opacity depend only on one coverage byte,
 * so a large bitmap can trade a 1280-byte stack table and a 256-entry
 * setup pass for fewer per-pixel multiplies.  The alpha-dependent
 * source color is premultiplied *after* the 0..255 alpha quantization
 * to match the original two-stage integer truncation exactly.
 */
typedef struct FT_Retro_Blend_LUT_
{
  FT_Byte  foreground[256][4];  /* BGRA */
  FT_Byte  inverse[256];

} FT_Retro_Blend_LUT;


static void
ft_bitmap_retro_blend_prepare( FT_Retro_Blend_LUT*  lut,
                               FT_Color             color )
{
  FT_UInt  aa;


  for ( aa = 0; aa < 256; ++aa )
  {
    FT_UInt  fa = ft_bitmap_retro_div255( (FT_UInt)color.alpha * aa );


    lut->foreground[aa][0] =
      (FT_Byte)ft_bitmap_retro_div255( (FT_UInt)color.blue  * fa );
    lut->foreground[aa][1] =
      (FT_Byte)ft_bitmap_retro_div255( (FT_UInt)color.green * fa );
    lut->foreground[aa][2] =
      (FT_Byte)ft_bitmap_retro_div255( (FT_UInt)color.red   * fa );
    lut->foreground[aa][3] = (FT_Byte)fa;
    lut->inverse[aa]       = (FT_Byte)( 255U - fa );
  }
}


static void
ft_bitmap_retro_blend_row_lut( FT_Byte*                   dst,
                               const FT_Byte*             coverage,
                               FT_UInt                    width,
                               const FT_Retro_Blend_LUT*  lut )
{
  FT_UInt  i;


  for ( i = 0; i < width; ++i, dst += 4 )
  {
    FT_UInt         aa = coverage[i];
    const FT_Byte*  foreground;
    FT_UInt         inverse;


    if ( !aa )
      continue;

    foreground = lut->foreground[aa];
    inverse    = lut->inverse[aa];

    dst[0] = (FT_Byte)( ft_bitmap_retro_div255( dst[0] * inverse ) +
                        foreground[0] );
    dst[1] = (FT_Byte)( ft_bitmap_retro_div255( dst[1] * inverse ) +
                        foreground[1] );
    dst[2] = (FT_Byte)( ft_bitmap_retro_div255( dst[2] * inverse ) +
                        foreground[2] );
    dst[3] = (FT_Byte)( ft_bitmap_retro_div255( dst[3] * inverse ) +
                        foreground[3] );
  }
}

#endif /* FT_CONFIG_OPTION_RETRO_BLEND_LUT */

#endif /* FTBITMAP_BLEND_RETRO_H_ */
