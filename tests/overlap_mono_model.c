/* Portable differential tests for exact 4x overlap grouping and full
 * MONO-span bytes.  Uses the real opt-in helper headers, but does not
 * execute R5900 MMI or SPARC VIS1 assembly.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char FT_Byte;
typedef unsigned int FT_UInt;

#define FT_MEM_SET( d, s, c )  memset( (d), (s), (size_t)(c) )
#include "../src/smooth/ftsmooth_retro_overlap.h"
#include "../src/raster/ftraster_retro_mono.h"

static uint32_t state = UINT32_C(0x47d2ac35);
static unsigned long overlap_cases, mono_cases, exhaustive_cases;

static uint32_t
next_random(void)
{
  uint32_t x = state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return state = x;
}

static void
original_overlap(FT_Byte* dst, unsigned x, unsigned len, unsigned coverage)
{
  unsigned int cover=(coverage+8U)/16U, i, sum;

  for(i=0;i<len;++i) {
    unsigned pixel=((unsigned short)x+i)/4U;
    sum=dst[pixel]+cover;
    dst[pixel]=(FT_Byte)(sum-(sum>>8));
  }
}

static void
original_mono(FT_Byte* dst, unsigned e1, unsigned e2)
{
  unsigned c1=e1>>3, c2=e2>>3;
  unsigned f1=0xFFU>>(e1&7);
  int f2=(~0x7F)>>(e2&7);
  unsigned d=c2-c1;
  FT_Byte* p=dst+c1;

  if(d>0) {
    p[0]|=(FT_Byte)f1;
    for(unsigned i=1;i<d;++i) p[i]=0xFF;
    p[d]|=(FT_Byte)f2;
  } else p[0]|=(FT_Byte)(f1&f2);
}

static void
opt_mono(FT_Byte* dst, unsigned e1, unsigned e2)
{
  unsigned c1=e1>>3, c2=e2>>3;
  unsigned f1=0xFFU>>(e1&7);
  int f2=(~0x7F)>>(e2&7);
  unsigned d=c2-c1;
  FT_Byte* p=dst+c1;

  if(d>0) {
    p[0]|=(FT_Byte)f1;
    if(d-1>=FT_RASTER_RETRO_MONO_MIN_BYTES)
      ft_raster_retro_mono_fill(p+1,(int)(d-1));
    else
      for(unsigned i=1;i<d;++i) p[i]=0xFF;
    p[d]|=(FT_Byte)f2;
  } else p[0]|=(FT_Byte)(f1&f2);
}

static void
run_overlap(unsigned x, unsigned length, unsigned coverage,
            unsigned overlap, unsigned pad)
{
  FT_Byte original[2048], candidate[2048];
  unsigned i;
  FT_Byte* a=original+32+pad;
  FT_Byte* b=candidate+32+pad;

  for(i=0;i<sizeof(original);++i)
    original[i]=(FT_Byte)next_random();
  memcpy(candidate,original,sizeof(candidate));

  original_overlap(a,x,length,coverage);
  ft_smooth_retro_overlap_span(b,x,length,coverage);

  if(overlap) {
    unsigned second=(coverage*37U+overlap*13U)&255U;
    original_overlap(a,x+overlap,length,second);
    ft_smooth_retro_overlap_span(b,x+overlap,length,second);
  }

  if(memcmp(original,candidate,sizeof(original))) {
    fprintf(stderr,"FAIL overlap x=%u length=%u coverage=%u overlap=%u pad=%u\n",
            x,length,coverage,overlap,pad);
    exit(1);
  }
  ++overlap_cases;
}

static void
run_mono(unsigned start, unsigned length, unsigned pad)
{
  FT_Byte original[768], candidate[768];
  unsigned i, end=start+length;
  FT_Byte *a=original+32+pad, *b=candidate+32+pad;

  for(i=0;i<sizeof(original);++i)
    original[i]=(FT_Byte)next_random();
  memcpy(candidate,original,sizeof(candidate));
  original_mono(a,start,end);
  opt_mono(b,start,end);

  if(memcmp(original,candidate,sizeof(original))) {
    fprintf(stderr,"FAIL MONO start=%u len=%u pad=%u\n",start,length,pad);
    exit(1);
  }
  ++mono_cases;
}

int
main(void)
{
  unsigned pixel, cover, count, iteration, length, x, pad;

  /* The grouping identity must hold for EVERY destination byte and
   * 1..4 repeated contributions of EVERY rounded coverage 0..16. */
  for(pixel=0;pixel<256;++pixel)
    for(cover=0;cover<=16;++cover)
      for(count=1;count<=4;++count) {
        FT_Byte actual=(FT_Byte)pixel;
        FT_Byte expected;
        for(iteration=0;iteration<count;++iteration) {
          unsigned s=actual+cover;
          actual=(FT_Byte)(s-(s>>8));
        }
        expected=ft_smooth_retro_overlap_add((FT_Byte)pixel,cover*count);
        if(actual!=expected) {
          fprintf(stderr,"FAIL grouped coverage pixel=%u cover=%u count=%u\n",
                  pixel,cover,count);
          return 1;
        }
        ++exhaustive_cases;
      }

  for(length=0;length<=260;++length)
    for(x=0;x<16;++x)
      for(pad=0;pad<8;++pad)
        run_overlap(x,length,(length*47U+x*17U+pad*31U)&255U,
                    (x+pad)%4U,pad);

  for(length=0;length<=1600;length+=5)
    for(x=0;x<32;++x)
      for(pad=0;pad<8;++pad)
        run_mono(x,length,pad);

  printf("PASS overlap/MONO: identity=%lu, overlap=%lu, MONO=%lu cases\n",
         exhaustive_cases,overlap_cases,mono_cases);
  return 0;
}
