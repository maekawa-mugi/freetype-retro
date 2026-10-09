/* Actual internal FT_Vector_NormLen function fingerprint.
 *
 * Build against a STATIC FreeType library and the matching internal
 * headers from the same source revision.  Normal shared-library API
 * builds may hide FT_BASE symbols.
 *
 * Use identical 32-bit ABI and compiler options for scalar and
 * target-specific builds; compare the entire output line.
 * The algorithm itself is not replaced by this optimization patch:
 * this checks that inlined FT_MSB / other arithmetic choices do not
 * change the normalized vector or its reported length.
 */
#include <ft2build.h>
#include <freetype/internal/ftcalc.h>

#include <stdint.h>
#include <stdio.h>
#include <limits.h>

static uint32_t state=UINT32_C(0x8a43f752);
static uint64_t hash=UINT64_C(14695981039346656037);
static unsigned long cases;

static uint32_t
random32(void)
{
  uint32_t x=state;
  x^=x<<13;
  x^=x>>17;
  x^=x<<5;
  return state=x;
}

static void
hash32(uint32_t v)
{
  int i;
  for(i=0;i<4;++i,v>>=8)
  {
    hash^=(unsigned char)v;
    hash*=UINT64_C(1099511628211);
  }
}

static void
check(int32_t x,int32_t y)
{
  FT_Vector vec;
  FT_UInt32 length;

  vec.x=(FT_Pos)x;
  vec.y=(FT_Pos)y;
  length=FT_Vector_NormLen(&vec);

  hash32((uint32_t)x);
  hash32((uint32_t)y);
  hash32((uint32_t)length);
  hash32((uint32_t)vec.x);
  hash32((uint32_t)vec.y);
  cases++;
}

int
main(void)
{
  static const int32_t values[]={
    -262144,-131072,-65537,-65536,-65535,-32769,
    -32768,-32767,-16384,-4096,-256,-64,-16,-2,-1,
    0,1,2,16,64,256,4096,16384,32767,32768,
    32769,65535,65536,65537,131072,262144
  };
  size_t i,j;

  if(sizeof(FT_Long)!=4)
  {
    fprintf(stderr,"NormLen fingerprint requires a 32-bit FT_Long\n");
    return 2;
  }

  for(i=0;i<sizeof(values)/sizeof(values[0]);i++)
    for(j=0;j<sizeof(values)/sizeof(values[0]);j++)
      check(values[i],values[j]);

  for(i=0;i<300000;i++)
  {
    uint32_t r1=random32()&UINT32_C(0x3ffff);
    uint32_t r2=random32()&UINT32_C(0x3ffff);
    int32_t x=(int32_t)r1;
    int32_t y=(int32_t)r2;

    if(random32()&1U)
      x=-x;
    if(random32()&1U)
      y=-y;

    check(x,y);
  }

  printf("FT_Vector_NormLen: %lu cases; FNV-1a-64 %016llx\n",
         cases,(unsigned long long)hash);
  return 0;
}
