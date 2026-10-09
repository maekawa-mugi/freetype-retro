/* Exact BGRA->GRAY weighted RGB component-square lookup model.
 *
 * Uses the actual helper, compares to the original FreeType formula,
 * and guards the entire destination buffer. Hardware ISA not used.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t FT_Byte;
typedef uint32_t FT_UInt32;
typedef unsigned int FT_UInt;
#include "../src/base/ftbitmap_bgra_gray_retro.h"

static uint32_t seed=UINT32_C(0x51bd8829);
static unsigned long rows,pixels;
static uint32_t
random32(void)
{
  uint32_t x=seed;
  x^=x<<13;
  x^=x>>17;
  x^=x<<5;
  return seed=x;
}
static FT_Byte
original_gray(const FT_Byte* p)
{
  FT_UInt a=p[3];
  FT_UInt l;

  if(!a)
    return 0;
  l=(4731UL*p[0]*p[0]+46868UL*p[1]*p[1]+
     13937UL*p[2]*p[2])>>16;
  return (FT_Byte)(a-l/a);
}
static void
check_row(const FT_Retro_BGRA_Gray_Table* table,
          FT_UInt width,unsigned offset,unsigned scheme)
{
  FT_Byte src[4*257+16],expected[257+32],actual[257+32];
  FT_UInt i;

  for(i=0;i<sizeof(src);i++)
    src[i]=(FT_Byte)random32();
  for(i=0;i<sizeof(expected);i++)
    expected[i]=(FT_Byte)random32();
  memcpy(actual,expected,sizeof(actual));

  for(i=0;i<width;i++)
  {
    FT_Byte* p=src+4*i;
    if(scheme==0)
      p[3]=0;
    else if(scheme==1)
      p[3]=1;
    else if(scheme==2)
      p[3]=255;
    else if(scheme==3)
    {
      p[3]=(FT_Byte)random32();
      p[0]=(FT_Byte)(p[0]%(p[3]+1U));
      p[1]=(FT_Byte)(p[1]%(p[3]+1U));
      p[2]=(FT_Byte)(p[2]%(p[3]+1U));
    }
    /* scheme 4 includes arbitrary, even non-premultiplied RGBA */
    expected[offset+i]=original_gray(p);
  }

  ft_bitmap_retro_bgra_gray_row(actual+offset,src,width,table);

  if(memcmp(expected,actual,sizeof(expected)))
  {
    fprintf(stderr,"FAIL BGRA gray row width=%u offset=%u scheme=%u\n",
            width,offset,scheme);
    exit(1);
  }
  rows++;
  pixels+=width;
}
int
main(void)
{
  FT_Retro_BGRA_Gray_Table table;
  unsigned i,scheme,k;

  ft_bitmap_retro_bgra_gray_prepare(&table);

  for(i=0;i<256;i++)
    for(scheme=0;scheme<5;scheme++)
    {
      FT_Byte src[4],got;
      src[0]=(FT_Byte)i;
      src[1]=(FT_Byte)(255U-i);
      src[2]=(FT_Byte)((i*13U)&255U);
      src[3]=scheme==0?0:scheme==1?1:
             scheme==2?128:scheme==3?255:(FT_Byte)i;
      ft_bitmap_retro_bgra_gray_row(&got,src,1,&table);
      if(got!=original_gray(src))
      {
        fprintf(stderr,"FAIL BGRA gray single pixel i=%u scheme=%u\n",i,scheme);
        return 1;
      }
      pixels++;
    }

  for(k=0;k<20000;k++)
  {
    FT_UInt width=(FT_UInt)(random32()%258U);
    unsigned offset=k%16U;

    for(scheme=0;scheme<5;scheme++)
      check_row(&table,width,offset,scheme);
  }

  printf("PASS BGRA grayscale LUT: %lu rows, %lu pixels\n",rows,pixels);
  return 0;
}
