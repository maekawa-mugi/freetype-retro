/* Baseline-vs-opt-in outline translation output fingerprint.
 *
 * Checks identity, x-only, y-only and two-axis translations,
 * including unsigned-wrapping FT_Pos additions at 32-bit bounds.
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t seed=UINT32_C(0xb67215d9);
static uint64_t hash=UINT64_C(14695981039346656037);
static unsigned long outlines,points;
static uint32_t
random32(void)
{
  uint32_t x=seed;
  x^=x<<13;
  x^=x>>17;
  x^=x<<5;
  return seed=x;
}
static void
hash32(uint32_t x)
{
  unsigned k;
  for(k=0;k<4;k++,x>>=8)
  {
    hash^=(unsigned char)x;
    hash*=UINT64_C(1099511628211);
  }
}
static void
check(unsigned n,unsigned offsets)
{
  FT_Vector actual[256],reference[256];
  FT_Outline outline;
  FT_Pos dx=0,dy=0;
  unsigned i;

  if(offsets==1)dy=64;
  else if(offsets==2)dx=0x10000L;
  else if(offsets==3)dx=-32769;
  else if(offsets==4)dy=-0x10000L;
  else if(offsets==5)
  {
    dx=0x10000L;
    dy=-0x20000L;
  }
  else if(offsets==6)
  {
    dx=(FT_Pos)(int32_t)UINT32_C(0x7fffffff);
    dy=(FT_Pos)(int32_t)UINT32_C(0x80000000);
  }

  for(i=0;i<n;i++)
  {
    actual[i].x=(FT_Pos)(int32_t)random32();
    actual[i].y=(FT_Pos)(int32_t)random32();
    reference[i].x=(FT_Pos)((FT_ULong)actual[i].x+(FT_ULong)dx);
    reference[i].y=(FT_Pos)((FT_ULong)actual[i].y+(FT_ULong)dy);
  }

  outline.n_points=(FT_Short)n;
  outline.n_contours=0;
  outline.points=actual;
  outline.tags=NULL;
  outline.contours=NULL;
  outline.flags=0;
  FT_Outline_Translate(&outline,dx,dy);

  for(i=0;i<n;i++)
  {
    if(actual[i].x!=reference[i].x ||
       actual[i].y!=reference[i].y)
    {
      fprintf(stderr,"FAIL outline translate n=%u offsets=%u i=%u\n",
              n,offsets,i);
      exit(1);
    }
    hash32((uint32_t)actual[i].x);
    hash32((uint32_t)actual[i].y);
  }

  hash32(n);
  hash32(offsets);
  points+=n;
  outlines++;
}
int
main(void)
{
  static const unsigned counts[]={
    0,1,2,3,4,7,8,15,16,17,31,32,33,63,64,65,127,128,255
  };
  unsigned i,k,j;

  if(sizeof(FT_Long)!=4)
  {
    fprintf(stderr,"Outline translate fingerprint requires 32-bit FT_Long\n");
    return 2;
  }

  for(i=0;i<sizeof(counts)/sizeof(counts[0]);i++)
    for(k=0;k<7;k++)
      for(j=0;j<128;j++)
        check(counts[i],k);

  printf("FT_Outline_Translate: %lu outlines %lu points FNV-1a-64=%016llx\n",
         outlines,points,(unsigned long long)hash);
  return 0;
}
