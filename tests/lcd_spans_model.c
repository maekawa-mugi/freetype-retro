/* Exact horizontal and vertical LCD 5-tap regression model.
 *
 * Uses the ACTUAL common folded span helper with the same byte-write
 * behavior as FreeType. No R5900 machine instructions are executed.
 * The MMI PADDB path needs a separate target instruction-level test.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char FT_Byte;
typedef unsigned int FT_UInt;
typedef int FT_Int;
typedef unsigned long FT_ULong;

#include "../src/smooth/ftsmooth_retro_lcd.h"

static uint32_t state = UINT32_C(0x2cc641a1);
static unsigned long cases;

static uint32_t
random32(void)
{
  uint32_t x = state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return state = x;
}

static void
ref_horizontal(FT_Byte* dst, unsigned len,
               FT_Byte coverage, const FT_Byte w[5])
{
  unsigned i, k;
  for (i=0;i<len;++i)
    for (k=0;k<5;++k)
      dst[i+k] = (FT_Byte)(dst[i+k] +
                         ((coverage*w[k]+85U)>>8));
}

static void
ref_vertical(FT_Byte* dst, unsigned len, int pitch,
             FT_Byte coverage, const FT_Byte w[5])
{
  int i;
  for (i=0;i<(int)len;++i) {
    dst[i          ] = (FT_Byte)(dst[i          ] + ((coverage*w[0]+85U)>>8));
    dst[i+pitch    ] = (FT_Byte)(dst[i+pitch    ] + ((coverage*w[1]+85U)>>8));
    dst[i+2*pitch  ] = (FT_Byte)(dst[i+2*pitch  ] + ((coverage*w[2]+85U)>>8));
    dst[i+3*pitch  ] = (FT_Byte)(dst[i+3*pitch  ] + ((coverage*w[3]+85U)>>8));
    dst[i+4*pitch  ] = (FT_Byte)(dst[i+4*pitch  ] + ((coverage*w[4]+85U)>>8));
  }
}

static void
run(unsigned len, unsigned offset, unsigned scenario,
    unsigned coverage_case, int negative_pitch)
{
  FT_Byte a[512*9], b[512*9], weights[5];
  FT_Byte *ref, *opt;
  FT_Byte cover;
  int pitch = negative_pitch ? -512 : 512;
  unsigned i;

  for (i=0;i<sizeof(a);++i)
    a[i]=(FT_Byte)random32();
  memcpy(b,a,sizeof(a));

  for(i=0;i<5;i++) {
    if(scenario==0) weights[i]=0;
    else if(scenario==1) weights[i]=255;
    else if(scenario==2) weights[i]=(FT_Byte)(i*59U+7U);
    else weights[i]=(FT_Byte)random32();
  }
  cover = coverage_case==0 ? 0 : coverage_case==1 ? 1 :
          coverage_case==2 ? 128 : coverage_case==3 ? 255 :
          (FT_Byte)random32();

  /* The LCD horizontal callback receives a span pointer that already
   * includes the two-pixel left filter border. */
  ref = a + 64 + offset;
  opt = b + 64 + offset;
  ref_horizontal(ref,len,cover,weights);
  ft_smooth_retro_lcd_horizontal(opt,len,cover,weights);

  /* Check overlap from a second raster span with a different coverage.
   * This checks modulo-256 behavior when accumulations overflow.
   */
  ref_horizontal(ref+2,len, (FT_Byte)(255U-cover),weights);
  ft_smooth_retro_lcd_horizontal(opt+2,len,(FT_Byte)(255U-cover),weights);
  if(memcmp(a,b,sizeof(a))) {
    fprintf(stderr,"FAIL horizontal len=%u offset=%u scenario=%u coverage=%u\n",
            len,offset,scenario,coverage_case);
    exit(1);
  }

  /* Separate, newly randomized rows for the vertical callback. */
  for(i=0;i<sizeof(a);++i)
    a[i]=(FT_Byte)random32();
  memcpy(b,a,sizeof(a));
  ref = a + (negative_pitch ? 7*512 : 1*512) + 64 + offset;
  opt = b + (negative_pitch ? 7*512 : 1*512) + 64 + offset;
  ref_vertical(ref,len,pitch,cover,weights);
  ft_smooth_retro_lcd_vertical(opt,len,cover,weights,pitch);
  ref_vertical(ref+3,len,pitch,(FT_Byte)(255U-cover),weights);
  ft_smooth_retro_lcd_vertical(opt+3,len,(FT_Byte)(255U-cover),weights,pitch);
  if(memcmp(a,b,sizeof(a))) {
    fprintf(stderr,"FAIL vertical len=%u offset=%u scenario=%u coverage=%u pitch=%d\n",
            len,offset,scenario,coverage_case,pitch);
    exit(1);
  }

  ++cases;
}

/*
 * Tight five-row allocation: both orientations use the boundary row
 * as the first tap, with no sixth row available beyond the last tap.
 * The optimized helper must not advance dst after its final write.
 */
static void
run_tight_vertical(unsigned len, unsigned negative)
{
  FT_Byte a[5*80], b[5*80];
  FT_Byte w[5] = { 8, 77, 86, 77, 8 };
  FT_Byte* ref;
  FT_Byte* opt;
  int pitch = negative ? -80 : 80;
  unsigned i;

  for (i=0;i<sizeof(a);++i)
    a[i]=(FT_Byte)random32();
  memcpy(b,a,sizeof(a));
  ref = a + (negative ? 4*80 : 0);
  opt = b + (negative ? 4*80 : 0);
  ref_vertical(ref,len,pitch,197,w);
  ft_smooth_retro_lcd_vertical(opt,len,197,w,pitch);
  if (memcmp(a,b,sizeof(a))) {
    fprintf(stderr,"FAIL tight LCD_V len=%u negative=%u\n",len,negative);
    exit(1);
  }
  ++cases;
}

int
main(void)
{
  unsigned len,off,scenario,cover;
  int pitch;

  for(len=0;len<=257;len++)
    for(off=0;off<16;off++)
      for(scenario=0;scenario<4;scenario++)
        for(cover=0;cover<5;cover++)
          for(pitch=0;pitch<2;pitch++)
            run(len,off,scenario,cover,pitch);

  /* Five rows exactly; no unused sixth-row pointer is permitted. */
  for(len=0;len<=80;len++)
    for(pitch=0;pitch<2;pitch++)
      run_tight_vertical(len,(unsigned)pitch);

  printf("PASS LCD five-tap folded span equivalence: %lu cases\n",cases);
  return 0;
}
