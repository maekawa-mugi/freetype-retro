/* Whole FreeType load/render/embolden timing. Link separately against
 * scalar and option-enabled libraries; use the identical font file.
 * Hash every glyph before timing; never hash, print, or look up chars
 * inside the measured batches. Six samples and per-size timings are
 * emitted as RG1 records, separate from the standalone RB1 kernels.
 */
#define _POSIX_C_SOURCE 200809L
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_EE)
#include <timer.h>
#else
#include <time.h>
#endif
#ifndef RETRO_BENCH_BUILD_ID
#define RETRO_BENCH_BUILD_ID "unlabelled"
#endif

static FT_UInt indices[512];
static unsigned count;
static volatile unsigned sink;
static uint64_t hash;

static uint64_t now(void)
{
#if defined(_EE)
  return GetTimerSystemTime();
#else
  struct timespec ts;
  if(clock_gettime(CLOCK_MONOTONIC,&ts))exit(2);
  return (uint64_t)ts.tv_sec*UINT64_C(1000000000)+ts.tv_nsec;
#endif
}
static void hash_byte(unsigned b)
{
  hash^=(unsigned char)b;
  hash*=UINT64_C(1099511628211);
}
static void hash_word(uint32_t v)
{
  unsigned j;
  for(j=0;j<4;j++,v>>=8)hash_byte(v);
}
static int batch(FT_Face face,FT_Render_Mode mode,int overlap,
                 unsigned strength,int fingerprint)
{
  unsigned i;
  for(i=0;i<count;i++){
    FT_GlyphSlot slot=face->glyph;
    FT_Bitmap* b=&slot->bitmap;
    size_t j,n;
    if(FT_Load_Glyph(face,indices[i],FT_LOAD_DEFAULT))return 0;
    if(overlap && slot->format==FT_GLYPH_FORMAT_OUTLINE)
      slot->outline.flags|=FT_OUTLINE_OVERLAP;
    if(FT_Render_Glyph(slot,mode))return 0;
    if(strength && FT_Bitmap_Embolden(face->glyph->library,b,
                                    (FT_Pos)strength*64,64))return 0;
    sink+=(unsigned)b->width;
    if(!fingerprint)continue;
    hash_word(indices[i]);
    hash_word((uint32_t)slot->advance.x);hash_word((uint32_t)slot->advance.y);
    hash_word((uint32_t)slot->bitmap_left);hash_word((uint32_t)slot->bitmap_top);
    hash_word(b->width);hash_word(b->rows);hash_word((uint32_t)b->pitch);
    hash_word(b->pixel_mode);hash_word(b->num_grays);
    n=(size_t)(b->pitch<0?-b->pitch:b->pitch)*b->rows;
    for(j=0;j<n;j++)hash_byte(b->buffer[j]);
  }
  return 1;
}
int main(int argc,char** argv)
{
  FT_Library lib;
  FT_Face face;
  FT_Render_Mode mode=FT_RENDER_MODE_NORMAL;
  unsigned sizes[]={8,12,18,32,64,128};
  unsigned si,s,r,reps=4,strength=0;
  FT_ULong cp;
  int overlap=0;
  uint64_t hz;
  if(argc<3 || argc>5){
    fprintf(stderr,"usage: %s font.ttf normal|mono|lcd|lcd-v|overlap [strength 0..4] [reps 1..100]\n",argv[0]);
    return 2;
  }
  if(!strcmp(argv[2],"mono"))mode=FT_RENDER_MODE_MONO;
  else if(!strcmp(argv[2],"lcd"))mode=FT_RENDER_MODE_LCD;
  else if(!strcmp(argv[2],"lcd-v"))mode=FT_RENDER_MODE_LCD_V;
  else if(!strcmp(argv[2],"overlap"))overlap=1;
  else if(strcmp(argv[2],"normal"))return 2;
  if(argc>3){char* end;unsigned long v=strtoul(argv[3],&end,10);
    if(!*argv[3] || *end || v>4)return 2;
    strength=(unsigned)v;}
  if(argc>4){char* end;unsigned long v=strtoul(argv[4],&end,10);
    if(!*argv[4] || *end || !v || v>100)return 2;
    reps=(unsigned)v;}
  if(FT_Init_FreeType(&lib))return 1;
  if(FT_New_Face(lib,argv[1],0,&face)){FT_Done_FreeType(lib);return 1;}
  for(cp=0x21;cp<=0x4e7f;cp++){
    FT_UInt index;
    if(!(cp<=0x7e || (cp>=0x3040 && cp<=0x30ff) || cp>=0x4e00))continue;
    index=FT_Get_Char_Index(face,cp);
    if(index)indices[count++]=index;
  }
  if(!count){FT_Done_Face(face);FT_Done_FreeType(lib);return 1;}
#if defined(_EE)
  hz=kBUSCLK;
#else
  hz=UINT64_C(1000000000);
#endif
  printf("RG1,META,%s,%s,%u,%u,%u,%llu\n",RETRO_BENCH_BUILD_ID,
         argv[2],strength,count,reps,(unsigned long long)hz);
  for(si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++){
    if(FT_Set_Pixel_Sizes(face,0,sizes[si]))goto fail;
    hash=UINT64_C(14695981039346656037);
    if(!batch(face,mode,overlap,strength,1))goto fail;
    printf("RG1,CHECK,%u,%016llx\n",sizes[si],(unsigned long long)hash);
    if(!batch(face,mode,overlap,strength,0))goto fail;
    for(s=0;s<6;s++){
      uint64_t begin=now(),elapsed;
      for(r=0;r<reps;r++)if(!batch(face,mode,overlap,strength,0))goto fail;
      elapsed=now()-begin;
      if(!elapsed)goto fail;
      printf("RG1,SAMPLE,%u,%u,%llu\n",sizes[si],s,(unsigned long long)elapsed);
    }
  }
  printf("RG1,DONE,PASS\n");
  FT_Done_Face(face);FT_Done_FreeType(lib);return 0;
fail:
  fprintf(stderr,"RG1,FATAL,render-or-timer\n");
  FT_Done_Face(face);FT_Done_FreeType(lib);return 1;
}
