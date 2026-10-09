/* Scalar-vs-lookup FT_Bitmap_Convert BGRA-to-GRAY fingerprint.
 *
 * The test covers the 4096-pixel activation threshold, positive and
 * negative bitmap pitches, padded rows and arbitrary alpha values.
 * Link independently to baseline and optimized FreeType libraries.
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t seed=UINT32_C(0x729ad813);
static uint64_t fingerprint=UINT64_C(14695981039346656037);
static unsigned long cases;

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
hash32(uint32_t v)
{
  unsigned i;
  for(i=0;i<4;i++,v>>=8)
  {
    fingerprint^=(unsigned char)v;
    fingerprint*=UINT64_C(1099511628211);
  }
}
static void
check(FT_Library lib,unsigned w,unsigned h,int negative,
      unsigned pad,unsigned scheme)
{
  FT_Bitmap source,target;
  unsigned stride=w*4+pad;
  size_t bytes=(size_t)stride*h;
  unsigned r,c;
  FT_Error err;

  FT_Bitmap_Init(&source);
  FT_Bitmap_Init(&target);
  source.buffer=(FT_Byte*)malloc(bytes);
  if(!source.buffer)
  {
    fprintf(stderr,"Cannot allocate BGRA test source\n");
    exit(2);
  }
  source.width=w;
  source.rows=h;
  source.pitch=negative?-(int)stride:(int)stride;
  source.pixel_mode=FT_PIXEL_MODE_BGRA;
  source.num_grays=256;

  for(r=0;r<h;r++)
  {
    FT_Byte* p=source.buffer+(size_t)r*stride;
    for(c=0;c<w;c++)
    {
      unsigned a=(unsigned)(random32()&255U);
      if(scheme==0)a=0;
      else if(scheme==1)a=1;
      else if(scheme==2)a=255;
      else if(scheme==3)a=128;

      p[c*4+3]=(FT_Byte)a;
      if(scheme==4)
      {
        p[c*4+0]=(FT_Byte)(random32()%(a+1U));
        p[c*4+1]=(FT_Byte)(random32()%(a+1U));
        p[c*4+2]=(FT_Byte)(random32()%(a+1U));
      }
      else
      {
        p[c*4+0]=(FT_Byte)random32();
        p[c*4+1]=(FT_Byte)random32();
        p[c*4+2]=(FT_Byte)random32();
      }
    }
    for(c=w*4;c<stride;c++)
      p[c]=(FT_Byte)random32();
  }

  /* An unset target pitch inherits the sign of the source. */
  err=FT_Bitmap_Convert(lib,&source,&target,4);
  if(err || target.width!=w || target.rows!=h ||
     target.pixel_mode!=FT_PIXEL_MODE_GRAY || target.num_grays!=256)
  {
    fprintf(stderr,"BGRA conversion failed w=%u h=%u sign=%d error=%d\n",
            w,h,negative,(int)err);
    exit(1);
  }
  hash32(w);
  hash32(h);
  hash32((uint32_t)target.pitch);
  hash32(scheme);

  /* Padding is not initialized by FT_Bitmap_Convert; hash only
   * active pixels, always reading the allocated memory layout.
   */
  for(r=0;r<h;r++)
  {
    const FT_Byte* p=target.buffer+(size_t)r*
                     (size_t)(target.pitch<0?-target.pitch:target.pitch);
    for(c=0;c<w;c++)
      hash32(p[c]);
  }
  FT_Bitmap_Done(lib,&target);
  free(source.buffer);
  cases++;
}
int
main(void)
{
  const unsigned widths[]={1,2,3,7,8,15,16,31,32,63,
                           64,127,128,255,256,257,511,512};
  const unsigned heights[]={1,2,3,7,8,16,17,32,64};
  FT_Library lib;
  unsigned i,j,k,p;
  int negative;

  if(FT_Init_FreeType(&lib))
  {
    fprintf(stderr,"FreeType initialization failed\n");
    return 1;
  }
  for(i=0;i<sizeof(widths)/sizeof(widths[0]);i++)
    for(j=0;j<sizeof(heights)/sizeof(heights[0]);j++)
      for(negative=0;negative<=1;negative++)
        for(p=0;p<2;p++)
          for(k=0;k<5;k++)
            check(lib,widths[i],heights[j],negative,p?7U:0U,k);

  FT_Done_FreeType(lib);
  printf("BGRA-to-GRAY convert: %lu cases; FNV-1a-64=%016llx\n",
         cases,(unsigned long long)fingerprint);
  return 0;
}
