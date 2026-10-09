/* Optional FT_MSB on-target output fingerprint.
 *
 * Build against otherwise identical scalar and optimized FreeType
 * trees for the same CPU/ABI. The target headers must match the target
 * library configuration. Nonzero inputs only: generic __builtin_clz(0)
 * is undefined, even though the old fallback FT_MSB(0) returns zero.
 */
#include <ft2build.h>
#include <freetype/internal/ftcalc.h>

#include <stdint.h>
#include <stdio.h>

static uint32_t state = UINT32_C(0xc81742eb);
static uint64_t hash = UINT64_C(14695981039346656037);
static unsigned long count;

static uint32_t
random32( void )
{
  uint32_t v = state;
  v ^= v << 13;
  v ^= v >> 17;
  v ^= v << 5;
  return state = v;
}

static void
hash_u32( uint32_t v )
{
  int j;
  for ( j = 0; j < 4; j++, v >>= 8 )
  {
    hash ^= (unsigned char)v;
    hash *= UINT64_C(1099511628211);
  }
}

static int
reference( uint32_t x )
{
  int msb;
  for ( msb=31; msb>0; --msb )
    if ( x & (UINT32_C(1) << msb) )
      break;
  return msb;
}

static int
check( uint32_t x )
{
  FT_Int got, expected;

  if ( !x )
    return 1;

  got = FT_MSB( (FT_UInt32)x );
  expected = reference(x);
  if ( got != expected )
  {
    fprintf(stderr, "FT_MSB mismatch: x=%08lx result=%d expected=%d\n",
            (unsigned long)x, got, expected);
    return 0;
  }
  hash_u32(x);
  hash_u32((uint32_t)got);
  count++;
  return 1;
}

int
main(void)
{
  unsigned i, bit;

  for (bit=0;bit<32;bit++)
  {
    uint32_t v = UINT32_C(1) << bit;
    if ( !check(v) || !check(v|1U) || !check(v-1U) )
      return 1;
  }

  for (i=0;i<131072;i++)
    if ( !check( i | UINT32_C(1) ) ||
         !check( (i << 16) | UINT32_C(1) ) ||
         !check( UINT32_C(0x80000000) | i ) )
      return 1;

  for (i=0;i<1000000;i++)
    if ( !check(random32()) )
      return 1;

  printf("FT_MSB: %lu inputs; FNV-1a-64 %016llx\n",
         count, (unsigned long long)hash );
  return 0;
}
