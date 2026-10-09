/* Fingerprint FT_Bitmap_Blend public API using initialized BGRA targets.
 *
 * Run against scalar and opt-in FreeType builds for the same CPU.  Hash
 * output data and logical metadata, not any timing.  Inputs use positive
 * pitch because the base FreeType code currently has empty negative-pitch
 * blend/copy branches (marked XXX).
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t seed = UINT32_C(0xc632811d);
static uint64_t digest = UINT64_C(14695981039346656037);
static unsigned long cases = 0;

static uint32_t
random32(void)
{
  uint32_t x = seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return seed = x;
}

static void
hash_byte(unsigned n)
{
  digest ^= (unsigned char)n;
  digest *= UINT64_C(1099511628211);
}

static void
hash_word(uint32_t n)
{
  int i;
  for (i=0;i<4;++i,n>>=8)
    hash_byte(n);
}

static int
case_run(FT_Library library, unsigned w, unsigned h,
         unsigned pad, unsigned color_id, unsigned coverage_pattern)
{
  FT_Bitmap src, target;
  FT_Vector src_offset, target_offset;
  FT_Color color;
  size_t pixels=(size_t)w*h, src_stride=w+pad, target_stride=w*4;
  FT_Error error;
  FT_Byte *source_bytes, *target_bytes;
  unsigned y,x;

  FT_Bitmap_Init(&src);
  FT_Bitmap_Init(&target);

  source_bytes=(FT_Byte*)malloc(src_stride*h);
  target_bytes=(FT_Byte*)malloc(target_stride*h);
  if (!source_bytes || !target_bytes) {
    free(source_bytes);
    free(target_bytes);
    fprintf(stderr,"out of memory\n");
    return 0;
  }

  for(y=0;y<h;++y) {
    for(x=0;x<w;++x) {
      FT_Byte value;
      if (coverage_pattern==0)
        value=0;
      else if (coverage_pattern==1)
        value=255;
      else if (coverage_pattern==2)
        value=(FT_Byte)(x+y*3);
      else
        value=(FT_Byte)random32();
      source_bytes[y*src_stride+x]=value;
    }
    for(x=w;x<src_stride;++x)
      source_bytes[y*src_stride+x]=0xC3;
  }
  for(x=0;x<target_stride*h;++x)
    target_bytes[x]=(FT_Byte)random32();

  src.width=w;
  src.rows=h;
  src.pitch=(int)src_stride;
  src.pixel_mode=FT_PIXEL_MODE_GRAY;
  src.num_grays=256;
  src.buffer=source_bytes;

  target.width=w;
  target.rows=h;
  target.pitch=(int)target_stride;
  target.pixel_mode=FT_PIXEL_MODE_BGRA;
  target.num_grays=256;
  target.buffer=target_bytes;

  color.blue=(FT_Byte)(color_id*31);
  color.green=(FT_Byte)(255-(color_id*17));
  color.red=(FT_Byte)(color_id*71);
  color.alpha=(FT_Byte)(color_id==0?0:color_id==1?1:
                        color_id==2?255:color_id==3?254:
                        color_id==4?128:color_id*29);

  src_offset.x=0;
  src_offset.y=(FT_Pos)h*64;
  target_offset=src_offset;

  error=FT_Bitmap_Blend(library,&src,src_offset,&target,&target_offset,color);
  if (error) {
    fprintf(stderr,"Blend error %d w=%u h=%u id=%u\n",error,w,h,color_id);
    free(source_bytes);
    free(target_bytes);
    return 0;
  }
  if(target.width != w || target.rows != h ||
     target.pixel_mode != FT_PIXEL_MODE_BGRA ||
     target.pitch != (int)target_stride ||
     target_offset.x != 0 || target_offset.y != (FT_Pos)h*64) {
    fprintf(stderr,"Blend changed target metadata unexpectedly\n");
    free(source_bytes);
    free(target_bytes);
    return 0;
  }

  hash_word(w);
  hash_word(h);
  hash_word(pad);
  hash_word(color_id);
  hash_word(coverage_pattern);
  hash_word((uint32_t)pixels);
  for(x=0;x<target_stride*h;++x)
    hash_byte(target_bytes[x]);

  free(source_bytes);
  free(target_bytes);
  ++cases;
  return 1;
}

int
main(void)
{
  FT_Library lib;
  const unsigned widths[]={1,2,7,15,16,17,31,32,33,63,64,65,
                           127,128,129,255,256,257,511,512};
  const unsigned heights[]={1,2,3,8,16,32};
  const unsigned pads[]={0,1,7};
  unsigned wi,he,pad,cov,col;

  if(FT_Init_FreeType(&lib)) {
    fprintf(stderr,"FT_Init_FreeType failed\n");
    return 1;
  }
  for(wi=0;wi<sizeof(widths)/sizeof(widths[0]);++wi)
    for(he=0;he<sizeof(heights)/sizeof(heights[0]);++he)
      for(pad=0;pad<sizeof(pads)/sizeof(pads[0]);++pad)
        for(col=0;col<8;++col)
          for(cov=0;cov<4;++cov)
            if(!case_run(lib,widths[wi],heights[he],pads[pad],col,cov)) {
              FT_Done_FreeType(lib);
              return 1;
            }

  FT_Done_FreeType(lib);
  printf("FT_Bitmap_Blend: %lu cases; FNV-1a-64 %016llx\n",
         cases,(unsigned long long)digest);
  return 0;
}
