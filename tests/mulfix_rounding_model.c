/* Host model for the exact R5900/SPARC32 FT_MulFix HI:LO rounding.
 *
 * Tests the ACTUAL shared rounding helper against FreeType's signed
 * 64-bit reference.  This file does not execute target multiplication
 * instructions; those remain in the deferred machine-level suite.
 */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int32_t  FT_Int32;
typedef uint32_t FT_UInt32;
#define FT_RETRO_MULFIX_MODEL_ONLY
#include "../include/freetype/internal/ftmulfix_retro.h"

static uint32_t seed = UINT32_C(0xa7459231);
static unsigned long checked;

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
check(int32_t a, int32_t b)
{
  int64_t product = (int64_t)a * (int64_t)b;
  int64_t rounded = product + (product < 0 ? 32767 : 32768);
  int32_t expected = (int32_t)(rounded >> 16);
  FT_UInt32 lo = (uint32_t)(uint64_t)product;
  FT_Int32 hi = (int32_t)((uint64_t)product >> 32);
  FT_Int32 got = ft_mulfix_retro_round_words(lo,hi);

  if(got != expected) {
    fprintf(stderr, "FT_MulFix model mismatch a=%ld b=%ld expected=%ld got=%ld\n",
            (long)a,(long)b,(long)expected,(long)got);
    exit(1);
  }
  checked++;
}

int
main(void)
{
  static const int32_t special[] = {
    INT32_MIN, INT32_MIN+1, -2000000000, -16777217, -16777216,
    -16777215, -131073, -131072, -131071, -65537, -65536,
    -65535, -32769, -32768, -32767, -2, -1, 0, 1, 2,
    32767, 32768, 32769, 65535, 65536, 65537, 131071,
    131072, 131073, 16777215, 16777216, 16777217,
    2000000000, INT32_MAX-1, INT32_MAX
  };
  size_t i,j;
  int32_t n;

  for(i=0;i<sizeof(special)/sizeof(special[0]);++i)
    for(j=0;j<sizeof(special)/sizeof(special[0]);++j)
      check(special[i],special[j]);

  /* Exhaustively cover negative/positive ties and small operands. */
  for(n=-65536;n<=65536;++n) {
    check(n,32768);
    check(n,-32768);
    check(n,65536);
    check(n,-65536);
    check(32768,n);
    check(-32768,n);
  }

  for(i=0;i<250000;++i)
    check((int32_t)random32(),(int32_t)random32());

  printf("PASS FT_MulFix 32-bit signed rounding model: %lu cases\n",checked);
  return 0;
}
