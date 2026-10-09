/* Differential model for both FT_MulDiv arithmetic configurations.
 *
 * Build once with RETRO_MULDIV_TEST_NO_INT64 undefined, and once with
 * -DRETRO_MULDIV_TEST_NO_INT64. The same actual helper header is used
 * both times. Test execution is intentionally deferred until batch.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef uint32_t FT_UInt32;
typedef int FT_Bool;
typedef int FT_Int;

#ifndef RETRO_MULDIV_TEST_NO_INT64
typedef uint64_t FT_UInt64;
#define FT_INT64 long long
#endif
#define FT_RETRO_MULDIV_MODEL_ONLY
static FT_UInt32
msb_model(FT_UInt32 v)
{
  FT_UInt32 i;
  for(i=31;i>0;--i)
    if(v & (UINT32_C(1)<<i))
      return i;
  return 0;
}
#define FT_MSB(v) msb_model(v)
#include "../src/base/ftmuldiv_retro.h"

static uint32_t seed=UINT32_C(0xe243f7b5);
static unsigned long checked,fast,unchanged,saturated;

static uint32_t
next_random(void)
{
  uint32_t x=seed;
  x^=x<<13;
  x^=x>>17;
  x^=x<<5;
  return seed=x;
}
static uint32_t
magnitude(int32_t x)
{
  return x<0 ? UINT32_C(0)-(uint32_t)x : (uint32_t)x;
}

/* Return the established quotient-magnitude behavior of the
 * corresponding existing FreeType implementation.  Signed
 * multiplication/division signs are checked independently below.
 */
static uint32_t
reference(uint32_t a,uint32_t b,uint32_t c,int rounding)
{
  uint64_t n;
  uint64_t q;
  uint32_t bias=c>>1;

  if(!c)
    return UINT32_C(0x7fffffff);

#ifdef RETRO_MULDIV_TEST_NO_INT64
  /* Original 32-bit code selects a direct multiply with an unsigned
   * a+b test, even if that sum wraps.  Preserve this odd corner case.
   */
  if((uint32_t)(a+b) <= (rounding ?
                         129894U-(c>>17) : 131071U))
  {
    uint32_t numerator=(uint32_t)(a*b);
    if(rounding)
      numerator+=bias;
    return numerator/c;
  }
#endif

  n=(uint64_t)a*b+(rounding?bias:0U);
  q=n/c;
#ifdef RETRO_MULDIV_TEST_NO_INT64
  if(q>UINT32_MAX)
    return UINT32_C(0x7fffffff);
#endif
  return (uint32_t)q;
}

static void
check(int32_t ia,int32_t ib,int32_t ic,int rounding)
{
  uint32_t a=magnitude(ia),b=magnitude(ib),c=magnitude(ic);
  uint32_t q=UINT32_C(0x1234abcd);
  uint32_t expected=reference(a,b,c,rounding);
  int selected=ft_muldiv_retro_fast32(a,b,c,rounding,&q);
  uint32_t expected_sign=( (ia<0) != (ib<0) ) != (ic<0) ?
                           UINT32_C(0)-expected : expected;

  if(selected)
  {
    uint32_t result_sign=( (ia<0) != (ib<0) ) != (ic<0) ?
                           UINT32_C(0)-q : q;
    if(q!=expected || result_sign!=expected_sign)
    {
      fprintf(stderr,
       "FAIL MulDiv fast32 a=%ld b=%ld c=%ld round=%d fast=%08lx expected=%08lx\n",
       (long)ia,(long)ib,(long)ic,rounding,
       (unsigned long)q,(unsigned long)expected);
      exit(1);
    }
    fast++;
  }
  else
    unchanged++;

  if(expected==UINT32_C(0x7fffffff))
    saturated++;
  checked++;
}

int
main(void)
{
  static const int32_t values[]={
    INT32_MIN,INT32_MIN+1,-1073741824,-262145,-262144,
    -131073,-131072,-131071,-65537,-65536,-65535,-32769,
    -32768,-32767,-256,-3,-2,-1,0,1,2,3,256,
    32767,32768,32769,65535,65536,65537,131071,
    131072,131073,262144,1073741824,INT32_MAX-1,
    INT32_MAX
  };
  unsigned i,j,k;
  uint32_t power;

  for(i=0;i<sizeof(values)/sizeof(values[0]);++i)
    for(j=0;j<sizeof(values)/sizeof(values[0]);++j)
      for(k=0;k<sizeof(values)/sizeof(values[0]);k++)
      {
        check(values[i],values[j],values[k],0);
        check(values[i],values[j],values[k],1);
      }

  for(k=0;k<32;k++)
  {
    power=UINT32_C(1)<<k;
    for(i=0;i<7000;i++)
    {
      int32_t a=(int32_t)next_random();
      int32_t b=(int32_t)next_random();
      check(a,b,(int32_t)power,0);
      check(a,b,(int32_t)power,1);
      check(a,b,(int32_t)(0U-power),0);
      check(a,b,(int32_t)(0U-power),1);
      check(a,a,(int32_t)power,1);
      check((int32_t)((uint32_t)a&0xffffU),b,
            (int32_t)power,1);
    }
  }

  for(i=0;i<550000;i++)
  {
    int32_t a=(int32_t)next_random();
    int32_t b=(int32_t)next_random();
    int32_t c=(int32_t)next_random();
    check(a,b,c,0);
    check(a,b,c,1);
    check((int32_t)((uint32_t)a&0xffffU),
          (int32_t)((uint32_t)b&0xffffU),c,1);
  }
#ifdef RETRO_MULDIV_TEST_NO_INT64
  printf("PASS MulDiv no-FT_INT64 fast32: %lu inputs fast=%lu fallback=%lu sentinel=%lu\n",
         checked,fast,unchanged,saturated);
#else
  printf("PASS MulDiv FT_INT64 fast32: %lu inputs fast=%lu fallback=%lu sentinel=%lu\n",
         checked,fast,unchanged,saturated);
#endif
  return 0;
}
