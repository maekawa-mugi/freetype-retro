/* Portable semantics comparison for actual FT_DivFix fast-path helper.
 *
 * No external FreeType library or target instructions are required.
 * This model validates every selected 32-bit fast path against
 * FT_DivFix's original exact unsigned-magnitude 64-bit numerator,
 * including negative input signs and 32-bit quotient wrap.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef uint32_t FT_UInt32;
typedef int FT_Bool;
typedef int FT_Int;

static int
test_msb( FT_UInt32 n )
{
  int i;
  for(i=31;i>0;--i)
    if(n & (UINT32_C(1)<<i))
      break;
  return i;
}
#define FT_MSB(v) test_msb(v)
#define FT_RETRO_DIVFIX_MODEL_ONLY
#include "../src/base/ftdivfix_retro.h"

static uint32_t state=UINT32_C(0x57e42b91);
static unsigned long cases=0;
static unsigned long fast=0, fallback=0, power2=0, bounded=0;

static uint32_t
random32(void)
{
  uint32_t x=state;
  x^=x<<13;
  x^=x>>17;
  x^=x<<5;
  return state=x;
}

static FT_UInt32
magnitude(int32_t x)
{
  return x<0 ? (FT_UInt32)0 - (FT_UInt32)x : (FT_UInt32)x;
}

static void
check(int32_t sa,int32_t sb)
{
  FT_UInt32 a=magnitude(sa),b=magnitude(sb),q=0;
  uint64_t original;
  FT_UInt32 expected,expected_signed,actual_signed;
  int has_fast,should_fast;

  has_fast=ft_divfix_retro_fast32(a,b,&q);
  should_fast=(b!=0 &&
               ( ( !(b&(b-1U)) &&
                   ( b>=65536U || a<(b<<16) ) ) ||
                 ( a<=65535U-(b>>17) ) ) );

  if(has_fast!=should_fast)
  {
    fprintf(stderr,"DivFix dispatch mismatch a=%ld b=%ld fast=%d expected=%d\n",
            (long)sa,(long)sb,has_fast,should_fast);
    exit(1);
  }

  if(has_fast)
  {
    original=((uint64_t)a*UINT64_C(65536)+(b>>1))/b;
    expected=(FT_UInt32)original;
    if(expected!=q)
    {
      fprintf(stderr,
              "DivFix magnitude mismatch a=%ld b=%ld got=%08lx expected=%08lx\n",
              (long)sa,(long)sb,(unsigned long)q,(unsigned long)expected);
      exit(1);
    }

    actual_signed=(sa<0) != (sb<0) ? (FT_UInt32)0 - q : q;
    expected_signed=(sa<0) != (sb<0) ? (FT_UInt32)0 - expected : expected;
    if(actual_signed!=expected_signed)
    {
      fprintf(stderr,"DivFix sign mismatch a=%ld b=%ld\n",(long)sa,(long)sb);
      exit(1);
    }
    fast++;
    if(!(b&(b-1U))) power2++;
    else bounded++;
  }
  else
    fallback++;

  cases++;
}

int
main(void)
{
  static const int32_t special[]={
    INT32_MIN,INT32_MIN+1,-1073741824,-65537,-65536,-65535,-32769,
    -32768,-32767,-16385,-16384,-16383,-2,-1,0,1,2,3,7,15,
    16383,16384,16385,32767,32768,32769,65535,65536,65537,
    131071,131072,1073741823,1073741824,INT32_MAX-1,INT32_MAX
  };
  size_t i,j;
  uint32_t bit;

  for(i=0;i<sizeof(special)/sizeof(special[0]);i++)
    for(j=0;j<sizeof(special)/sizeof(special[0]);j++)
      check(special[i],special[j]);

  /* Explicitly keep the oversized quotient in the original path:
   * a=32768, b=1 yields an unsigned quotient of 0x80000000;
   * a=65536, b=1 yields 0x100000000 and must NOT use the shortcut.
   * The two baseline FT_INT64 configurations handle oversized
   * quotients differently, so no global shortcut may rewrite them.
   */
  check(32768,1);
  check(65535,1);
  check(65536,1);
  check(INT32_MIN,1);
  check(INT32_MIN,-1);
  check(65536,2);
  check(131072,2);
  check(INT32_MIN,32768);

  for(bit=0;bit<32;bit++)
  {
    uint32_t v=UINT32_C(1)<<bit;
    for(i=0;i<10000;i++)
    {
      uint32_t a=random32();
      int32_t signed_a=(int32_t)a;
      int32_t signed_b=(int32_t)v;
      check(signed_a,signed_b);
      check(signed_a,(int32_t)(0U-v));
      check((int32_t)(a&65535U),signed_b);
    }
  }

  /* Dense low-numerator cases exercise the 32/32 shortcut; large
   * operands frequently select the original 64-bit fallback.
   */
  for(i=0;i<500000;i++)
  {
    int32_t a=(int32_t)random32();
    int32_t b=(int32_t)random32();

    check(a,b);
    check((int32_t)((FT_UInt32)a&65535U),b);
  }

  printf("PASS FT_DivFix fast32 model: %lu cases; fast=%lu (power2=%lu bounded=%lu), fallback=%lu\n",
         cases,fast,power2,bounded,fallback);
  return 0;
}
