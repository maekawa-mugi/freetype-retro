/* Public FT_MulFix fingerprint for baseline vs optimized libraries.
 *
 * Build and run separately against the same FreeType source revision,
 * first with the retro MulFix switch off and then with it on.
 * Compare entire output lines on the SAME 32-bit target architecture.
 */
#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

static uint32_t seed = UINT32_C(0x9e7bb192);
static uint64_t hash = UINT64_C(14695981039346656037);
static unsigned long cases;

static uint32_t
random32(void)
{
  uint32_t x = seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return seed = x;
}

static void
hash_byte(unsigned char byte)
{
  hash ^= byte;
  hash *= UINT64_C(1099511628211);
}

static void
hash_word(uint32_t v)
{
  int i;
  for(i=0;i<4;i++,v>>=8)
    hash_byte((unsigned char)v);
}

static void
check(FT_Long a, FT_Long b)
{
  /* Parentheses avoid a possible internal inlining macro, so the
   * public FT_MulFix symbol is exercised independently. */
  FT_Long result = (FT_MulFix)(a,b);

  hash_word((uint32_t)(int32_t)a);
  hash_word((uint32_t)(int32_t)b);
  hash_word((uint32_t)(int32_t)result);
  cases++;
}

int
main(void)
{
  static const int32_t special[]={
    INT32_MIN, INT32_MIN+1, -2000000000, -16777217, -16777216,
    -16777215, -131073, -131072, -131071, -65537, -65536,
    -65535, -32769, -32768, -32767, -2, -1, 0, 1, 2,
    32767, 32768, 32769, 65535, 65536, 65537, 131071,
    131072, 131073, 16777215, 16777216, 16777217,
    2000000000, INT32_MAX-1, INT32_MAX
  };
  size_t i,j;
  int n;

  for(i=0;i<sizeof(special)/sizeof(special[0]);++i)
    for(j=0;j<sizeof(special)/sizeof(special[0]);++j)
      check(special[i],special[j]);

  for(n=-65536;n<=65536;++n) {
    check(n,32768);
    check(n,-32768);
    check(n,65536);
    check(n,-65536);
  }

  for(i=0;i<250000;i++)
    check((int32_t)random32(),(int32_t)random32());

  printf("FT_MulFix: %lu cases; FNV-1a-64 %016llx\n",
         cases,(unsigned long long)hash);
  return 0;
}
