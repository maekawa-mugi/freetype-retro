/* Baseline-vs-fast-outline deterministic FreeType API fingerprint.
 *
 * The oracle executes the exported FT_Vector_Transform once per point,
 * and compares against the actual FT_Outline_Transform function.
 * Compile against otherwise identical scalar and optimized libraries.
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t seed=UINT32_C(0x869af317);
static uint64_t hash=UINT64_C(14695981039346656037);
static unsigned long outlines,points_checked;
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
static FT_Pos
coordinate(void)
{
  uint32_t x=random32() & UINT32_C(0x003fffff);
  return (FT_Pos)((random32()&1U) ? -(int32_t)x : (int32_t)x);
}
static void
get_matrix(FT_Matrix* m,unsigned type)
{
  m->xx=0x10000L;
  m->xy=0;
  m->yx=0;
  m->yy=0x10000L;

  if(type==1)
  {
    m->xx=0x20000L;
    m->yy=0x8000L;
  }
  else if(type==2)
  {
    m->xx=-0x10000L;
    m->yy=-0x10000L;
  }
  else if(type==3)
  {
    m->xy=0x4000L;
    m->yx=-0x2000L;
  }
  else if(type==4)
  {
    m->xx=0;
    m->xy=-0x10000L;
    m->yx=0x10000L;
    m->yy=0;
  }
  else if(type==5)
  {
    m->xx=0;
    m->yy=0;
  }
  else if(type==6)
  {
    m->xx=(FT_Fixed)((int32_t)(random32()&UINT32_C(0x1ffff))-65536);
    m->xy=(FT_Fixed)((int32_t)(random32()&UINT32_C(0x1ffff))-65536);
    m->yx=(FT_Fixed)((int32_t)(random32()&UINT32_C(0x1ffff))-65536);
    m->yy=(FT_Fixed)((int32_t)(random32()&UINT32_C(0x1ffff))-65536);
  }
}
static void
check(unsigned n,unsigned kind)
{
  FT_Vector src[256],actual[256],expected[256];
  FT_Outline outline;
  FT_Matrix matrix;
  unsigned j;

  get_matrix(&matrix,kind);
  for(j=0;j<n;j++)
  {
    src[j].x=coordinate();
    src[j].y=coordinate();
    expected[j]=src[j];
    actual[j]=src[j];
  }
  outline.n_points=(FT_Short)n;
  outline.n_contours=0;
  outline.points=actual;
  outline.tags=NULL;
  outline.contours=NULL;
  outline.flags=0;

  FT_Outline_Transform(&outline,&matrix);
  for(j=0;j<n;j++)
  {
    FT_Vector_Transform(&expected[j],&matrix);
    if(expected[j].x!=actual[j].x || expected[j].y!=actual[j].y)
    {
      fprintf(stderr,
              "FAIL Outline_Transform n=%u kind=%u j=%u\n",n,kind,j);
      exit(1);
    }
    hash32((uint32_t)actual[j].x);
    hash32((uint32_t)actual[j].y);
  }

  hash32(n);
  hash32(kind);
  outlines++;
  points_checked+=n;
}
int
main(void)
{
  static const unsigned lengths[]={
    0,1,2,3,4,7,8,15,16,17,31,32,33,63,64,65,127,128,255
  };
  unsigned i,k,r;

  if(sizeof(FT_Long)!=4)
  {
    fprintf(stderr,"Outline transform fingerprint requires 32-bit FT_Long\n");
    return 2;
  }
  for(i=0;i<sizeof(lengths)/sizeof(lengths[0]);i++)
    for(k=0;k<7;k++)
      for(r=0;r<144;r++)
        check(lengths[i],k);

  printf("FT_Outline_Transform: %lu outlines %lu points FNV-1a-64=%016llx\n",
         outlines,points_checked,(unsigned long long)hash);
  return 0;
}
