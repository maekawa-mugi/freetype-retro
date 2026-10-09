/* FT_MSB semantics model, no R5900 or SPARC executable required.
 *
 * This compiles the ACTUAL opt-in SPARC32 De Bruijn helper and models
 * the documented lower-32-bit R5900 PLZCW count.  It compares both
 * to a small portable reference, including an explicit zero case.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t  FT_Byte;
typedef int      FT_Int;
typedef uint32_t FT_UInt32;
#define FT_SIZEOF_INT  4
#define FT_SIZEOF_LONG 4
#define FT_CONFIG_OPTION_RETRO_MSB_SPARC32
#define FT_RETRO_MSB_MODEL_ONLY
#include "../include/freetype/internal/ftmsb_retro.h"

static uint32_t state = UINT32_C(0xb879612d);
static unsigned long cases;

static uint32_t
random32( void )
{
  uint32_t x = state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return state = x;
}

static int
reference( uint32_t value )
{
  int bit;

  for ( bit = 31; bit > 0; bit-- )
    if ( value & (UINT32_C(1) << bit) )
      return bit;

  return 0;
}

/* PLZCW reports leading-bit-sign-run length minus one, separately
 * for the lower and upper 32-bit source lanes.  FT_MSB only uses
 * the lower 32-bit lane of a positive nonzero source word.
 */
static int
plzcw_word_model( uint32_t value )
{
  int count = 0;
  uint32_t mask = UINT32_C(0x40000000);

  if ( !value )
    return 0;
  if ( value & UINT32_C(0x80000000) )
    return 31;

  while ( mask && !( value & mask ) )
  {
    ++count;
    mask >>= 1;
  }

  return 30 - count;
}

static void
check( uint32_t x )
{
  int expected = reference(x);
  int sparc = ft_msb_retro_sparc32(x);
  int ee = plzcw_word_model(x);

  if (sparc != expected || ee != expected)
  {
    fprintf(stderr,
            "FT_MSB mismatch x=%08lx ref=%d ee=%d sparc=%d\n",
            (unsigned long)x, expected, ee, sparc);
    exit(1);
  }
  ++cases;
}

int
main(void)
{
  unsigned bit, i;
  uint32_t v;

  check(0);
  check(UINT32_MAX);
  for(bit=0;bit<32;bit++)
  {
    v = UINT32_C(1) << bit;
    check(v);
    check(v-1);
    check(v+ (v != UINT32_MAX ? 1U : 0U));
    check(~v);
  }

  /* Exhaustive low-16 range, with the 16 possible sign and
   * higher-bit patterns interleaved for upper-word coverage. */
  for(i=0;i<65536;i++)
  {
    unsigned k;
    for(k=0;k<16;k++)
      check((uint32_t)i | ((uint32_t)(k * 0x1111U) << 16));
  }

  for(i=0;i<1000000;i++)
    check(random32());

  printf("PASS FT_MSB EE PLZCW/SPARC DeBruijn model: %lu cases\n",cases);
  return 0;
}
