/****************************************************************************
 *
 * ftdivmul_r5900.h
 *
 *   Optional exact FT_DivFix / FT_MulDiv 64/32 division built from 32-bit
 *   DIVU operations (for PS2 EE and other 32-bit FT_Long targets).
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTDIVMUL_R5900_H_
#define FTDIVMUL_R5900_H_

#ifndef FT_RETRO_DIVMUL_MODEL_ONLY
#if FT_SIZEOF_LONG != 4
#error "FT_CONFIG_OPTION_RETRO_DIVMUL_DIGITWISE requires 32-bit FT_Long"
#endif
#endif

/*
 * Low 32 bits of floor( n / z ) for a 64-bit numerator n and a nonzero
 * 32-bit divisor z -- exactly what FreeType's FT_INT64 code keeps when
 * it truncates the quotient to FT_Long.  No 64-bit division is used:
 *
 *   hi == 0                 one 32/32 division
 *   z < 2^(32-bitlen(hi))   two 32/32 divisions (see below)
 *   otherwise               Hacker's Delight `divlu' (two DIVs and
 *                           multiplies)
 *
 * Writing n = hi*2^32 + lo and hi = k*z + r gives
 *   floor( n / z ) = k*2^32 + floor( ( r*2^32 + lo ) / z ),
 * the second quotient being below 2^32, so reducing hi modulo z first
 * does not change the low 32 bits of the result.
 */
static FT_UInt32
ft_divmul_r5900_clz( FT_UInt32  v )          /* v != 0 */
{
#if defined( FT_RETRO_DIVMUL_MODEL_ONLY ) || !defined( _MIPS_ARCH_R5900 )
  return (FT_UInt32)__builtin_clz( v );
#else
  FT_UInt32  c;


  /* PLZCW counts leading sign bits minus one; shifting right once makes */
  /* the operand non-negative so the count equals clz( v ) for v >= 2.   */
  /* It also writes the count of the upper word into bits 63..32, which  */
  /* the compiler does not expect in a 32-bit value (a later `c ? ..'    */
  /* test would see it), so `sll 0' re-canonicalizes the register.       */
  __asm__ ( "plzcw  %0, %1\n\tsll    %0, %0, 0"
            : "=r" ( c ) : "r" ( v >> 1 ) );
  return c;
#endif
}

/* u1 < v, v >= 65536 */
static FT_UInt32
ft_divmul_r5900_divlu( FT_UInt32  u1,
                       FT_UInt32  u0,
                       FT_UInt32  v )
{
  const FT_UInt32  b = 65536U;
  FT_UInt32  s = ft_divmul_r5900_clz( v );
  FT_UInt32  vn1, vn0, un32, un10, un1, un0, q1, q0, rhat, un21;


  v   <<= s;
  vn1  = v >> 16;
  vn0  = v & 0xFFFFU;
  un32 = s ? ( u1 << s ) | ( u0 >> ( 32U - s ) ) : u1;
  un10 = u0 << s;
  un1  = un10 >> 16;
  un0  = un10 & 0xFFFFU;

  q1   = un32 / vn1;
  rhat = un32 - q1 * vn1;
  while ( q1 >= b || q1 * vn0 > b * rhat + un1 )
  {
    q1--;
    rhat += vn1;
    if ( rhat >= b )
      break;
  }

  un21 = un32 * b + un1 - q1 * v;
  q0   = un21 / vn1;
  rhat = un21 - q0 * vn1;
  while ( q0 >= b || q0 * vn0 > b * rhat + un0 )
  {
    q0--;
    rhat += vn1;
    if ( rhat >= b )
      break;
  }

  return q1 * b + q0;
}

/* 32/32 division with quotient and remainder from one DIVU.  The divisor
 * is known to be nonzero, so the assembler's zero check is not wanted. */
static void
ft_divmul_r5900_divu( FT_UInt32   n,
                      FT_UInt32   d,
                      FT_UInt32*  q,
                      FT_UInt32*  r )
{
#if defined( FT_RETRO_DIVMUL_MODEL_ONLY ) || !defined( _MIPS_ARCH_R5900 )
  *q = n / d;
  *r = n % d;
#else
  FT_UInt32  lo, hi;


  __asm__ ( "divu $zero, %2, %3\n\tmflo %0\n\tmfhi %1"
            : "=r" ( lo ), "=r" ( hi ) : "r" ( n ), "r" ( d )
            : "hi", "lo" );
  *q = lo;
  *r = hi;
#endif
}

/*
 * With s = bit length of hi, z < 2^(32-s) allows
 *
 *   m  = n >> s            (fits 32 bits)
 *   q1 = m / z,  r1 = m % z
 *   q  = ( q1 << s ) + ( ( r1 << s ) | ( n & ( 2^s - 1 ) ) ) / z
 *
 * i.e. two DIVUs and no correction loops.  Larger divisors use `divlu'.
 */
static FT_UInt32
ft_divmul_r5900_div64( FT_UInt64  n,
                       FT_UInt32  z )
{
  FT_UInt32  hi = (FT_UInt32)( n >> 32 );
  FT_UInt32  lo = (FT_UInt32)n;
  FT_UInt32  q1, r1, q2, r2, c, s, m;


  if ( !hi )
  {
    ft_divmul_r5900_divu( lo, z, &q1, &r1 );
    return q1;
  }

  c = ft_divmul_r5900_clz( hi );
  if ( z >> c )
    return ft_divmul_r5900_divlu( hi < z ? hi : hi % z, lo, z );

  s = 32U - c;
  m = ( hi << c ) | ( lo >> s );
  ft_divmul_r5900_divu( m, z, &q1, &r1 );
  m = ( r1 << s ) | ( lo & ( ( 1U << s ) - 1U ) );
  ft_divmul_r5900_divu( m, z, &q2, &r2 );

  return ( q1 << s ) + q2;
}

#endif /* FTDIVMUL_R5900_H_ */
