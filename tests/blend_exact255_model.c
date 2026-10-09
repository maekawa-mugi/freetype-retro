/* Exact premultiplied BGRA blending semantics test.
 *
 * Can run without FreeType or an architecture-specific cross compiler.
 * Defines only the small internal types required by the opt-in row helper.
 * Checks mathematical identity and compares the actual header routines
 * against the original FT_Bitmap_Blend operation sequence.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int FT_UInt;
typedef unsigned char FT_Byte;
typedef struct FT_Color_ {
  FT_Byte blue, green, red, alpha;
} FT_Color;

#define FT_CONFIG_OPTION_RETRO_BLEND_LUT
#include "../src/base/ftbitmap_blend_retro.h"

static uint32_t seed = UINT32_C(0x9a741bee);
static unsigned long tests = 0;

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
reference(FT_Byte* dst, const FT_Byte* mask, unsigned width, FT_Color color)
{
  unsigned i;
  for (i = 0; i < width; ++i, dst += 4) {
    int aa = mask[i];
    int fa = color.alpha * aa / 255;
    int fb = color.blue  * fa / 255;
    int fg = color.green * fa / 255;
    int fr = color.red   * fa / 255;
    int inv = 255 - fa;
    int bb = dst[0], bg = dst[1], br = dst[2], ba = dst[3];

    dst[0] = (FT_Byte)(bb * inv / 255 + fb);
    dst[1] = (FT_Byte)(bg * inv / 255 + fg);
    dst[2] = (FT_Byte)(br * inv / 255 + fr);
    dst[3] = (FT_Byte)(ba * inv / 255 + fa);
  }
}

static FT_Color
make_color(unsigned n)
{
  FT_Color c;
  if (n < 8U) {
    static const FT_Byte test_colors[8][4] = {
      { 0, 0, 0, 0 },       { 255, 255, 255, 255 },
      { 255, 0, 0, 1 },     { 0, 255, 0, 254 },
      { 0, 0, 255, 128 },   { 1, 254, 128, 127 },
      { 197, 39, 251, 73 },{ 7, 131, 83, 253 }
    };
    c.blue  = test_colors[n][0];
    c.green = test_colors[n][1];
    c.red   = test_colors[n][2];
    c.alpha = test_colors[n][3];
  } else {
    c.blue = (FT_Byte)next_random();
    c.green = (FT_Byte)next_random();
    c.red = (FT_Byte)next_random();
    c.alpha = (FT_Byte)next_random();
  }
  return c;
}

static void
check(unsigned width, unsigned offset, FT_Color color, unsigned pattern)
{
  FT_Byte source[4096 + 32];
  FT_Byte expected[4 * (4096 + 32)];
  FT_Byte direct[4 * (4096 + 32)];
  FT_Byte lut_output[4 * (4096 + 32)];
  FT_Retro_Blend_LUT lut;
  FT_Byte *a = expected + 64 + offset;
  FT_Byte *b = direct + 64 + offset;
  FT_Byte *c = lut_output + 64 + offset;
  unsigned i;

  memset(expected, 0xA3, sizeof(expected));
  memcpy(direct, expected, sizeof(expected));
  memcpy(lut_output, expected, sizeof(expected));
  for(i=0;i<width;++i) {
    switch(pattern) {
      case 0: source[i]=0; break;
      case 1: source[i]=255; break;
      case 2: source[i]=(FT_Byte)i; break;
      case 3: source[i]=(FT_Byte)(i % 2 ? 255 : 0); break;
      default: source[i]=(FT_Byte)next_random(); break;
    }
    a[4*i+0] = b[4*i+0] = c[4*i+0] = (FT_Byte)next_random();
    a[4*i+1] = b[4*i+1] = c[4*i+1] = (FT_Byte)next_random();
    a[4*i+2] = b[4*i+2] = c[4*i+2] = (FT_Byte)next_random();
    a[4*i+3] = b[4*i+3] = c[4*i+3] = (FT_Byte)next_random();
  }
  reference(a, source, width, color);
  ft_bitmap_retro_blend_row(b, source, width, color);
  ft_bitmap_retro_blend_prepare(&lut, color);
  ft_bitmap_retro_blend_row_lut(c, source, width, &lut);
  if (memcmp(expected,direct,sizeof(expected)) ||
      memcmp(expected,lut_output,sizeof(expected))) {
    fprintf(stderr, "FAIL width=%u offset=%u pattern=%u alpha=%u\n",
            width, offset, pattern, color.alpha);
    exit(1);
  }
  ++tests;
}

int
main(void)
{
  unsigned n, k, color, pattern, off;
  const unsigned widths[] = {
    1, 2, 3, 4, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65,
    127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024, 2048, 4096
  };

  for (n=0; n<=65025U; ++n) {
    if (ft_bitmap_retro_div255(n) != n / 255U) {
      fprintf(stderr, "FAIL div255 at n=%u\n", n);
      return 1;
    }
  }
  for (k=0;k<sizeof(widths)/sizeof(widths[0]);++k)
    for (off=0;off<16;++off)
      for (color=0;color<16;++color)
        for (pattern=0;pattern<5;++pattern)
          check(widths[k],off,make_color(color),pattern);

  printf("PASS blend-model: div255 inputs=65026, row-cases=%lu\n", tests);
  return 0;
}
