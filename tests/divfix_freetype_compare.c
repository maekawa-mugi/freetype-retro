/* Scalar-vs-fast32 FT_DivFix fingerprint for the 32-bit ABI.
 *
 * Compile separately against scalar and opted-in FreeType libraries
 * made from the same revision/configuration/target/compiler.  This
 * checks the public FT_DivFix symbol, including signed, zero divisor,
 * large quotient and rounding-boundary inputs.
 */
#include <ft2build.h>
#include FT_FREETYPE_H

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t state = UINT32_C(0xab39817d);
static uint64_t hash = UINT64_C(14695981039346656037);
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
hash_word(uint32_t value)
{
  unsigned i;
  for(i=0;i<4;i++,value>>=8)
  {
    hash^=(unsigned char)value;
    hash*=UINT64_C(1099511628211);
  }
}

static void
check(int32_t a,int32_t b)
{
  FT_Long result=FT_DivFix((FT_Long)a,(FT_Long)b);

  hash_word((uint32_t)a);
  hash_word((uint32_t)b);
  hash_word((uint32_t)result);
  ++cases;
}

int
main(void)
{
  static const int32_t special[]={
    INT32_MIN,INT32_MIN+1,-1073741824,-65537,-65536,-65535,
    -32769,-32768,-32767,-16385,-16384,-16383,-2,-1,
    0,1,2,3,7,15,16383,16384,16385,32767,32768,32769,
    65535,65536,65537,131071,131072,1073741823,1073741824,
    INT32_MAX-1,INT32_MAX
  };
  size_t i,j;
  uint32_t bit;

  if(sizeof(FT_Long)!=4)
  {
    fprintf(stderr,"The fast32 DivFix comparison requires 32-bit FT_Long\n");
    return 2;
  }

  for(i=0;i<sizeof(special)/sizeof(special[0]);i++)
    for(j=0;j<sizeof(special)/sizeof(special[0]);j++)
      check(special[i],special[j]);

  for(bit=0;bit<32;bit++)
  {
    uint32_t b=UINT32_C(1)<<bit;
    for(i=0;i<2000;i++)
    {
      int32_t a=(int32_t)random32();
      check(a,(int32_t)b);
      check(a,(int32_t)(0U-b));
      check((int32_t)((uint32_t)a&65535U),(int32_t)b);
    }
  }

  for(i=0;i<400000;i++)
  {
    int32_t a=(int32_t)random32();
    int32_t b=(int32_t)random32();
    check(a,b);
    check((int32_t)((uint32_t)a&65535U),b);
  }

  printf("FT_DivFix: %lu cases; FNV-1a-64 %016llx\n",
         cases,(unsigned long long)hash);
  return 0;
}
