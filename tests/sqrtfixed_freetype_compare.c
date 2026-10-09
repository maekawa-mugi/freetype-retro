/* FT_SqrtFixed baseline-vs-opt-in target fingerprint.
 *
 * This function is an internal FT_BASE symbol; link a matching STATIC
 * FreeType archive and use its matching internal source-tree headers.
 * Compare the output line between otherwise identical baseline and
 * FT_CONFIG_OPTION_RETRO_SQRT_RESTORING builds on the same 32-bit ABI.
 *
 * In the baseline, FT_INT64 must be enabled for the 24-step option
 * to be active.  The test includes a zero input, all small values,
 * square-root transitions, extremes and deterministic random values.
 */
#include <ft2build.h>
#include <freetype/internal/ftcalc.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t seed = UINT32_C(0x702c81bd);
static uint64_t digest = UINT64_C(14695981039346656037);
static unsigned long cases;

static uint32_t
next_random(void)
{
  uint32_t x = seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return seed = x;
}

static void
hash_u32(uint32_t v)
{
  unsigned i;

  for (i=0;i<4;i++,v>>=8)
  {
    digest ^= (unsigned char)v;
    digest *= UINT64_C(1099511628211);
  }
}

static void
check(uint32_t v)
{
  FT_UInt32 q = FT_SqrtFixed((FT_UInt32)v);

  hash_u32(v);
  hash_u32((uint32_t)q);
  cases++;
}

int
main(void)
{
  static const uint32_t special[] =
  {
    0,1,2,3,4,5,6,7,15,16,17,63,64,65,
    127,128,129,255,256,257,1023,1024,1025,
    32767,32768,32769,65535,65536,65537,
    131071,131072,UINT32_C(0x7fffffff),
    UINT32_C(0x80000000),UINT32_C(0xfffffffe),UINT32_MAX
  };
  unsigned i,k;

  if (sizeof(FT_Long)!=4)
  {
    fprintf(stderr,"FT_SqrtFixed restoring test requires 32-bit FT_Long\n");
    return 2;
  }

  for(i=0;i<sizeof(special)/sizeof(special[0]);++i)
    check(special[i]);

  for(i=0;i<65536U;++i)
    check(i);

  for(k=0;k<32;++k)
  {
    uint32_t v = UINT32_C(1) << k;
    check(v);
    check(v-1U);
    check(v+1U);
    check(~v);
  }

  for(i=0;i<65536U;++i)
  {
    uint64_t root = (uint64_t)i * 256U;
    uint32_t v = (uint32_t)((root*root)>>16);
    check(v);
    check(v ? v-1U : 0U);
    check(v==UINT32_MAX ? v : v+1U);
  }

  for(i=0;i<250000U;++i)
    check(next_random());

  printf("FT_SqrtFixed: %lu inputs; FNV-1a-64 %016llx\n",
         cases,(unsigned long long)digest);
  return 0;
}
