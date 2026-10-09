/* FreeType API MONO embolden LUT fingerprint for large row spans.
 *
 * Exercise widths/rows on both sides of the 1024-byte threshold,
 * including negative source pitch, row padding and strengths 2..8.
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t seed=UINT32_C(0x4e8a3297);
static uint64_t hash=UINT64_C(14695981039346656037);
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
    hash^=(unsigned char)v;
    hash*=UINT64_C(1099511628211);
  }
}
static void
check(FT_Library lib,unsigned width,unsigned rows,
      unsigned pad,unsigned strength,int negative)
{
  FT_Bitmap src,dst;
  unsigned pitch=(width+7U)/8U+pad;
  unsigned i,r;
  FT_Error err;

  FT_Bitmap_Init(&src);
  FT_Bitmap_Init(&dst);
  src.buffer=(FT_Byte*)malloc((size_t)pitch*rows);
  if(!src.buffer)
  {
    fprintf(stderr,"MONO test allocation failed\n");
    exit(2);
  }
  for(i=0;i<pitch*rows;i++)
    src.buffer[i]=(FT_Byte)random32();

  src.width=width;
  src.rows=rows;
  src.pixel_mode=FT_PIXEL_MODE_MONO;
  src.num_grays=2;
  src.pitch=negative?-(int)pitch:(int)pitch;
  err=FT_Bitmap_Copy(lib,&src,&dst);
  if(err)
  {
    fprintf(stderr,"MONO test copy failed error=%d\n",(int)err);
    exit(1);
  }

  err=FT_Bitmap_Embolden(lib,&dst,(FT_Pos)strength*64,0);
  if(err || dst.width!=width+strength || dst.rows!=rows)
  {
    fprintf(stderr,
      "MONO test embolden failed w=%u h=%u strength=%u err=%d\n",
      width,rows,strength,(int)err);
    exit(1);
  }

  hash32(width);
  hash32(rows);
  hash32(strength);
  hash32((uint32_t)dst.pitch);
  for(r=0;r<dst.rows;r++)
  {
    unsigned stride=(unsigned)(dst.pitch<0?-dst.pitch:dst.pitch);
    const FT_Byte* row=dst.buffer+(size_t)r*stride;
    unsigned active=(dst.width+7U)/8U;
    for(i=0;i<active;i++)
      hash32(row[i]);
  }
  FT_Bitmap_Done(lib,&dst);
  free(src.buffer);
  cases++;
}
int
main(void)
{
  const unsigned widths[]={1,7,8,15,16,31,32,63,64,127,128,
                           255,256,511,512,1024};
  const unsigned heights[]={1,2,4,8,16,32,64,128};
  const unsigned pads[]={0,1,7};
  FT_Library lib;
  unsigned i,j,p,str;
  int negative;

  if(FT_Init_FreeType(&lib))
  {
    fprintf(stderr,"FreeType initialization failed\n");
    return 1;
  }

  for(i=0;i<sizeof(widths)/sizeof(widths[0]);i++)
    for(j=0;j<sizeof(heights)/sizeof(heights[0]);j++)
      for(p=0;p<sizeof(pads)/sizeof(pads[0]);p++)
        for(str=2;str<=8;str++)
          for(negative=0;negative<=1;negative++)
            check(lib,widths[i],heights[j],pads[p],str,negative);

  FT_Done_FreeType(lib);
  printf("MONO embolden: %lu cases; FNV-1a-64=%016llx\n",
         cases,(unsigned long long)hash);
  return 0;
}
