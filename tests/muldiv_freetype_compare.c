/* Baseline-vs-opt-in FT_MulDiv public and no-round internal API
 * fingerprints. Both functions are compared against matching
 * FreeType STATIC archives and include trees on the same 32-bit ABI.
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/internal/ftcalc.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t seed=UINT32_C(0xa458f671);
static uint64_t fingerprint=UINT64_C(14695981039346656037);
static unsigned long cases;

static uint32_t
random32(void)
{
  uint32_t x=seed;
  x^=x<<13;
  x^=x>>17;
  x^=x<<5;
  return seed=x;
}
static void
hash32(uint32_t x)
{
  unsigned i;
  for(i=0;i<4;i++,x>>=8)
  {
    fingerprint^=(unsigned char)x;
    fingerprint*=UINT64_C(1099511628211);
  }
}
static void
check(int32_t a,int32_t b,int32_t c)
{
  FT_Long rounded=FT_MulDiv((FT_Long)a,(FT_Long)b,(FT_Long)c);
  FT_Long truncated=FT_MulDiv_No_Round((FT_Long)a,(FT_Long)b,(FT_Long)c);

  hash32((uint32_t)a);
  hash32((uint32_t)b);
  hash32((uint32_t)c);
  hash32((uint32_t)rounded);
  hash32((uint32_t)truncated);
  cases++;
}
int
main(void)
{
  static const int32_t v[]={
    INT32_MIN,INT32_MIN+1,-1073741824,-131072,-65537,
    -65536,-65535,-32769,-32768,-32767,-1,0,1,
    2,3,32767,32768,32769,65535,65536,65537,
    131072,1073741824,INT32_MAX-1,INT32_MAX
  };
  unsigned i,j,k;

  if(sizeof(FT_Long)!=4)
  {
    fprintf(stderr,"MulDiv fingerprint requires 32-bit FT_Long\n");
    return 2;
  }

  for(i=0;i<sizeof(v)/sizeof(v[0]);i++)
    for(j=0;j<sizeof(v)/sizeof(v[0]);j++)
      for(k=0;k<sizeof(v)/sizeof(v[0]);k++)
        check(v[i],v[j],v[k]);

  for(k=0;k<32;k++)
  {
    uint32_t c=UINT32_C(1)<<k;
    for(i=0;i<4000;i++)
    {
      int32_t a=(int32_t)random32(),b=(int32_t)random32();
      check(a,b,(int32_t)c);
      check(a,b,(int32_t)(0U-c));
      check((int32_t)((uint32_t)a&0xffffU),b,(int32_t)c);
    }
  }

  for(i=0;i<300000;i++)
    check((int32_t)random32(),(int32_t)random32(),(int32_t)random32());

  printf("FT_MulDiv/No_Round: %lu inputs FNV-1a-64=%016llx\n",
         cases,(unsigned long long)fingerprint);
  return 0;
}
