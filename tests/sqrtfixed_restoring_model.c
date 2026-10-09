/* FT_SqrtFixed restoring algorithm model (host C only).
 *
 * Compares the ACTUAL 24-step 32-bit helper with two independent
 * references: the FreeType FT_INT64 Babylonian recurrence and a
 * 64-bit integer binary-search oracle with nearest-root rounding.
 * It does not compile or execute R5900 MMI / SPARC VIS1 instructions.
 */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned int FT_UInt;
typedef uint32_t FT_UInt32;
#define FT_RETRO_SQRT_MODEL_ONLY
#include "../src/base/ftsqrtrestro.h"

static uint32_t seed = UINT32_C(0xf23a4197);
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

static FT_UInt
reference_msb(FT_UInt32 v)
{
  FT_UInt bit;
  for (bit=31;bit>0;--bit)
    if (v & (UINT32_C(1) << bit))
      return bit;
  return 0;
}

/* Transcription of FreeType's existing FT_INT64 branch, including
 * its -1 on the dividend and the rounded-up Newton iteration.
 */
static FT_UInt32
reference_babylonian(FT_UInt32 v)
{
  uint64_t r;
  FT_UInt32 q,t;

  if (!v)
    return 0;

  r = ((uint64_t)v << 16) - 1U;
  q = UINT32_C(1) << ((17U + reference_msb(v)) >> 1);

  do
  {
    t = q;
    q = (t + (FT_UInt32)(r / t) + 1U) >> 1;
  } while (q != t);

  return q;
}

/* An independently implemented exact integer sqrt oracle.
 * The rounded root is floor(sqrt(v*65536)) + (remainder > root).
 * All products of midpoint values fit in unsigned 64 bits.
 */
static FT_UInt32
reference_binary_search(FT_UInt32 v)
{
  uint64_t n = (uint64_t)v << 16;
  FT_UInt32 lo = 0, hi = UINT32_C(1) << 24;

  while (lo + 1U < hi)
  {
    FT_UInt32 mid = lo + ((hi - lo) >> 1);
    uint64_t square = (uint64_t)mid * mid;

    if (square <= n)
      lo = mid;
    else
      hi = mid;
  }

  return lo + ((n - (uint64_t)lo * lo) > lo);
}

static void
check(FT_UInt32 v)
{
  FT_UInt32 got = ft_sqrt_retro_restoring(v);
  FT_UInt32 babylonian = reference_babylonian(v);
  FT_UInt32 mathematical = reference_binary_search(v);

  if (got != babylonian || got != mathematical)
  {
    fprintf(stderr,
            "FAIL SqrtFixed v=0x%08lx restoring=%lu Babylonian=%lu exact=%lu\n",
            (unsigned long)v,(unsigned long)got,
            (unsigned long)babylonian,(unsigned long)mathematical);
    exit(1);
  }
  cases++;
}

int
main(void)
{
  static const FT_UInt32 special[] =
  {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 15U, 16U, 17U,
    63U, 64U, 65U, 127U, 128U, 129U, 255U, 256U,
    257U, 1023U, 1024U, 1025U, 32767U, 32768U,
    32769U, 65535U, 65536U, 65537U, 131071U,
    131072U, UINT32_C(0x7fffffff), UINT32_C(0x80000000),
    UINT32_C(0xfffffffe), UINT32_MAX
  };
  unsigned i, bit;

  for(i=0;i<sizeof(special)/sizeof(special[0]);++i)
    check(special[i]);

  /* Exhaustive low 16-bit inputs, including the FT_INT64 small
   * Babylonian path's historically delicate sub-unit values. */
  for(i=0;i<65536U;i++)
    check(i);

  for(bit=0;bit<32;bit++)
  {
    FT_UInt32 v = UINT32_C(1) << bit;

    check(v);
    check(v - 1U);
    check(v + 1U);
    check(~v);
  }

  /* Near roots of 128*i, spread across the full 24-bit root domain.
   * These target close-to-perfect-square and rounding transitions.
   */
  for(i=0;i<131072U;i++)
  {
    uint64_t root = (uint64_t)i * 128U;
    FT_UInt32 v = (FT_UInt32)((root * root) >> 16);

    check(v);
    check(v ? v - 1U : v);
    check(v == UINT32_MAX ? v : v + 1U);
  }

  for(i=0;i<1000000U;i++)
    check(random32());

  printf("PASS SqrtFixed restoring vs Babylonian / exact oracle: %lu cases\n",
         cases);
  return 0;
}
