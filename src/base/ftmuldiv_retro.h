/****************************************************************************
 *
 * ftmuldiv_retro.h
 *
 *   Optional exact fast cases for FT_MulDiv / FT_MulDiv_No_Round.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTMULDIV_RETRO_H_
#define FTMULDIV_RETRO_H_

#ifndef FT_RETRO_MULDIV_MODEL_ONLY
#if FT_SIZEOF_LONG != 4
#error "FT_CONFIG_OPTION_RETRO_MULDIV_FAST32 requires 32-bit FT_Long"
#endif
#endif

/*
 * Return a nonzero flag if the unsigned-magnitude quotient has been
 * computed using an exactly equivalent inexpensive expression.
 *
 * All arguments are magnitudes of signed 32-bit FT_Long values.
 * In particular, a, b and c are <= 0x80000000, even for INT_MIN.
 * Signedness and the division-by-zero sentinel stay at the caller.
 *
 * When FT_INT64 exists, FreeType calculates its 64-bit numerator and
 * truncates its quotient to FT_Long; the power-of-two shortcut
 * deliberately preserves that low-32-bit behavior.
 *
 * Without FT_INT64, FreeType has a 32-bit direct path which can be
 * selected after an UNSIGNED WRAPPING a+b test, and a slow path that
 * saturates huge quotients.  Keep these existing corner behaviors:
 * only select shortcuts proven to yield the same result from the
 * original direct path, and never rewrite cases where a+b overflows.
 */
static FT_Bool
ft_muldiv_retro_fast32( FT_UInt32   a,
                        FT_UInt32   b,
                        FT_UInt32   c,
                        FT_Bool     rounding,
                        FT_UInt32*  result )
{
  FT_UInt32  limit;


  if ( !c )
    return 0;

  if ( !a || !b )
  {
    *result = 0;
    return 1;
  }

  /* Avoid accidentally changing the historical no-FT_INT64 direct
   * branch for inputs where its a+b check wraps modulo 2^32.
   */
  if ( a > 0xFFFFFFFFUL - b )
    return 0;

  /* A common scaled-geometry case: multiplying by the divisor
   * returns the other multiplier exactly, with either rounding rule.
   */
  if ( a == c || b == c )
  {
    *result = a == c ? b : a;
    return 1;
  }

  limit = rounding ? 129894U - ( c >> 17 ) : 131071U;

  /* This is the established FreeType bound.  It proves that the
   * full 32-bit unsigned numerator (with rounding if requested)
   * does not overflow.  The no-FT_INT64 baseline already has this
   * branch; selecting it is primarily helpful on FT_INT64 builds.
   */
  if ( a + b <= limit )
  {
    *result = ( a * b + ( rounding ? ( c >> 1 ) : 0U ) ) / c;
    return 1;
  }

  if ( !( c & ( c - 1U ) ) )
  {
    FT_UInt32  shift = (FT_UInt32)FT_MSB( c );

#ifdef FT_INT64

    /* 32x32->64 multiplication then a dynamic shift: no 64/32
     * integer division.  The result cast matches FT_MulDiv's
     * existing 32-bit FT_Long truncation, including large values.
     */
    FT_UInt64  product = (FT_UInt64)a * b;


    if ( rounding )
      product += c >> 1;

    *result = (FT_UInt32)( product >> shift );
    return 1;

#else /* !FT_INT64 */

    /* Only use this shortcut when its ENTIRE rounded numerator
     * fits in 32 bits, and the baseline a+b condition above
     * selected a guaranteed overflow-free direct calculation.
     * For other values, leave the emulated 64-bit behavior intact.
     */
    if ( a <= 32767U && b <= 32767U )
    {
      *result = ( a * b + ( rounding ? ( c >> 1 ) : 0U ) ) >> shift;
      return 1;
    }

#endif /* FT_INT64 */
  }

  return 0;
}

#endif /* FTMULDIV_RETRO_H_ */
