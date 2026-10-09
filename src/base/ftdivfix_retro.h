/****************************************************************************
 *
 * ftdivfix_retro.h
 *
 *   Optional exact 32-bit FT_DivFix fast paths.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTDIVFIX_RETRO_H_
#define FTDIVFIX_RETRO_H_

/* Supported 32-bit FT_Long builds only.  On wider ABIs FT_DivFix
 * accepts values too large for these fast-path operands.
 */
#ifndef FT_RETRO_DIVFIX_MODEL_ONLY
#if FT_SIZEOF_LONG != 4
#error "FT_CONFIG_OPTION_RETRO_DIVFIX_FAST32 requires 32-bit FT_Long"
#endif
#endif

/*
 * Try to evaluate the unsigned magnitude of
 *
 *   floor( (a*65536 + floor(b/2)) / b )
 *
 * exactly.  Caller handles signed magnitude conversion and the
 * existing division-by-zero sentinel.  Return 0 when the original
 * FreeType division routine should be used instead.
 *
 * q is intentionally only 32 bits.  The original FT_DivFix casts its
 * quotient to FT_Long (32 bits) before applying the sign, so even
 * cases with an oversized mathematical quotient keep their low 32
 * bits.  All arithmetic here uses unsigned 32-bit values.
 */
static FT_Bool
ft_divfix_retro_fast32( FT_UInt32   a,
                        FT_UInt32   b,
                        FT_UInt32*  q )
{
  FT_UInt32  k;


  if ( !b )
    return 0;

  /* Exact shift-based rounded quotient for b=2^k, k=0..31.
   * For k<=16, the dividend is divisible by b without a remainder.
   * For k>16, add half a unit before the right shift, as FreeType
   * does when adding (b>>1) before integer division.
   */
  if ( !( b & ( b - 1U ) ) )
  {
    k = (FT_UInt32)FT_MSB( b );

    if ( k <= 16 )
      *q = a << ( 16 - k );
    else
      *q = ( a + ( 1U << ( k - 17 ) ) ) >> ( k - 16 );

    return 1;
  }

  /* Conservative FreeType-style bound for keeping the ENTIRE
   * unrounded product and half-divisor inside 32 bits.
   *
   * b is at most 0x80000000 on a signed 32-bit ABI.  Therefore
   * 65535 - (b>>17) is nonnegative.  This bound is inherited from
   * the pre-existing non-FT_INT64 implementation of FT_DivFix.
   */
  if ( a <= 65535U - ( b >> 17 ) )
  {
    *q = ( ( a << 16 ) + ( b >> 1 ) ) / b;
    return 1;
  }

  return 0;
}

#endif /* FTDIVFIX_RETRO_H_ */
