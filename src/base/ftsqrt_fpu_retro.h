/****************************************************************************
 *
 * ftsqrt_fpu_retro.h
 *
 *   Optional FT_SqrtFixed using the single-precision FPU square root
 *   plus an exact integer fix-up (for PS2 EE R5900).
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTSQRT_FPU_RETRO_H_
#define FTSQRT_FPU_RETRO_H_

#ifndef FT_RETRO_SQRT_FPU_MODEL_ONLY
#if FT_SIZEOF_LONG != 4
#error "FT_CONFIG_OPTION_RETRO_SQRT_FPU requires 32-bit FT_Long"
#endif
#endif

/*
 * round( sqrt( v * 65536 ) ) for v != 0.
 *
 * A float estimate of 256*sqrt(2)*sqrt(v>>1) is within a few units of
 * the true root (the 24-bit float significand is far finer than the
 * result's integer step at this magnitude); two integer loops then pull
 * it to floor( sqrt( n ) ), n = v << 16, and the remainder test rounds:
 * the nearest integer is root+1 exactly when n - root^2 > root, as
 * (root + 1/2)^2 = root^2 + root + 1/4 is never an integer.
 *
 * Input is converted as a signed 31-bit value (v >> 1), so the estimate
 * is computed for v/2 and scaled by sqrt(2)*256 -- no unsigned convert.
 */
static FT_UInt32
ft_sqrt_fpu_retro( FT_UInt32  v )
{
  FT_UInt64  n   = (FT_UInt64)v << 16;
  float      f   = (float)(FT_Int32)( v >> 1 );
  float      s;
  FT_UInt64  t;
  FT_UInt32  r;


#if defined( FT_RETRO_SQRT_FPU_MODEL_ONLY ) || !defined( _MIPS_ARCH_R5900 )
  s = __builtin_sqrtf( f );
#else
  __asm__ ( "sqrt.s  %0, %1" : "=f" ( s ) : "f" ( f ) );
#endif
  r = (FT_UInt32)(FT_Int32)( s * 362.03867196751236f );

  /* The rounded root is the smallest R with R * ( R + 1 ) >= n, so a  */
  /* single widening product, updated incrementally, replaces the      */
  /* three products of a floor-root and remainder fix-up.              */
  t = (FT_UInt64)r * ( r + 1U );
  while ( t < n )
  {
    r++;
    t += 2ULL * r;
  }
  while ( t - 2ULL * r >= n )
  {
    t -= 2ULL * r;
    r--;
  }

  return r;
}

#endif /* FTSQRT_FPU_RETRO_H_ */
