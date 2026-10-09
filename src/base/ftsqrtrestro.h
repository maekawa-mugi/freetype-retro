/****************************************************************************
 *
 * ftsqrt_retro.h
 *
 *   Optional division-free 16.16 square root for 32-bit targets.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTSQRT_RETRO_H_
#define FTSQRT_RETRO_H_

#ifndef FT_RETRO_SQRT_MODEL_ONLY
#if FT_SIZEOF_LONG != 4
#error "FT_CONFIG_OPTION_RETRO_SQRT_RESTORING requires 32-bit FT_Long"
#endif
#endif

/*
 * Compute round(sqrt(v * 65536)) using 32-bit unsigned integer
 * arithmetic only.  This is a binary restoring square root of the
 * 48-bit value [v:16 zero bits], consumed as 24 base-4 digits.
 *
 * The first 16 digits come from v; the remaining eight are zero.
 * At each step:
 *
 *   remainder = 4 * remainder + next_digit
 *   trial     = 4 * partial_root + 1
 *   if remainder >= trial:
 *       remainder -= trial, partial_root = 2*partial_root + 1
 *   else:
 *       partial_root = 2*partial_root
 *
 * After 24 steps, partial_root is floor(sqrt(v * 65536)) and
 * remainder is (v * 65536) - partial_root^2.  The nearest integer
 * rounds up exactly when remainder > partial_root because:
 *
 *    (partial_root + 1/2)^2 = partial_root^2 + partial_root + 1/4.
 *
 * No integer input lands exactly on that half-unit boundary.
 * The maximum root is 2^24-1; trial and remainder fit in 32 bits.
 * Original FreeType's FT_INT64 Babylonian algorithm rounds to the
 * same result.  The non-FT_INT64 implementation is not replaced.
 *
 * The opt-in algorithm has 24 fixed iterations, no divisions,
 * no 64-bit multiplication, and no hardware-specific instructions.
 * Whether it is faster depends on the target compiler and CPU.
 */
static FT_UInt32
ft_sqrt_retro_restoring( FT_UInt32  value )
{
  FT_UInt32  stream    = value;
  FT_UInt32  root      = 0;
  FT_UInt32  remainder = 0;
  FT_UInt    i;


  if ( !value )
    return 0;

  /* Consume the most significant 32 input bits, two at a time. */
  for ( i = 0; i < 16; ++i )
  {
    FT_UInt32  trial = ( root << 2 ) + 1U;


    remainder = ( remainder << 2 ) | ( stream >> 30 );
    stream  <<= 2;
    root     <<= 1;

    if ( remainder >= trial )
    {
      remainder -= trial;
      root++;
    }
  }

  /* Sixteen implied low zero bits, consumed eight pairs at a time. */
  for ( i = 0; i < 8; ++i )
  {
    FT_UInt32  trial = ( root << 2 ) + 1U;


    remainder <<= 2;
    root      <<= 1;

    if ( remainder >= trial )
    {
      remainder -= trial;
      root++;
    }
  }

  return root + ( remainder > root );
}

#endif /* FTSQRT_RETRO_H_ */
