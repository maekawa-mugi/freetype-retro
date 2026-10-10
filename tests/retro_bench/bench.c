/*
 * FreeType retro A/B/C benchmark + correctness gate.
 * Harness design follows openssl-retro test/ps2/bench.c + main.c:
 * separated correctness, deterministic operands, rotating order,
 * identical workloads, six samples, and digest AFTER timer stop.
 *
 * Output: RB1,META|CHECK|SAMPLE,... (CSV for verdict.py).
 * This source executes NO unit tests unless the operator runs it.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "kernels.h"

#if defined(RETRO_BENCH_R5900)
#include <kernel.h>
#include <timer.h>
#include <debug.h>  /* PS2SDK GS debug screen: init_scr/scr_printf */
#define RB_TARGET "r5900"
#elif defined(RETRO_BENCH_SPARC32)
#define RB_TARGET "sparc32"
#else
#define RB_TARGET "host"
#endif

#ifndef RETRO_BENCH_BUILD_ID
#define RETRO_BENCH_BUILD_ID "unlabelled"
#endif
#define RB_SAMPLES 6
#define RB_ITEMS 1024U
#define RB_MAX 8192U
#define RB_OUT_SIZE (RB_MAX*4U+128U)

static FT_UInt32 a[RB_ITEMS],b[RB_ITEMS],c[RB_ITEMS],result[RB_ITEMS];
#if defined(__GNUC__)
#define RB_ALIGNED __attribute__((aligned(16)))
#else
#define RB_ALIGNED
#endif
static FT_Byte input[RB_OUT_SIZE] RB_ALIGNED;
static FT_Byte initial[RB_OUT_SIZE] RB_ALIGNED;
static FT_Byte output[RB_OUT_SIZE] RB_ALIGNED;
/* Benchmark use is aligned; pre-timer correctness also exercises
 * offsets 17..20, which trigger target prefix/tail scalar fallbacks. */
static unsigned rb_offset=16U;
static FT_Retro_Blend_LUT blend_table;
static FT_Retro_BGRA_Gray_Table gray_table;
static FT_Retro_Mono_Embolden_Table mono_table;
static volatile FT_UInt32 escape_sink;
/* A real per-call lookup table build is part of COLD timing.
 * Volatile function pointers prevent whole-loop hoisting by the
 * compiler without changing the computation performed by the helper.
 */
static void (*volatile cold_blend_prepare)(FT_Retro_Blend_LUT*,FT_Color)
    = ft_bitmap_retro_blend_prepare;
static void (*volatile cold_gray_prepare)(FT_Retro_BGRA_Gray_Table*)
    = ft_bitmap_retro_bgra_gray_prepare;
static void (*volatile cold_mono_prepare)(FT_Retro_Mono_Embolden_Table*,FT_UInt)
    = ft_bitmap_retro_mono_embolden_prepare;

enum { MSB,MULFIX,DIVFIX,MULDIV,MULDIV_NO,SQRT,LCD,LCD_V,LCD_V_NEG,BLEND,BLEND_COLD,
       BGRA,BGRA_COLD,MONO,MONO_COLD,OVERLAP,GRAYFILL,ROW_OR,
       PACK_MONO,PACK_GRAY2,PACK_GRAY4,EMBOLDEN_GRAY8,
       EMBOLDEN_GRAY8_X2,EMBOLDEN_GRAY8_X3,EMBOLDEN_GRAY8_X4 };
struct rb_case { const char* name; int kind; unsigned size; unsigned reps; unsigned offset; unsigned alpha_mode; };
static const struct rb_case cases[] = {
  {"msb-1024",MSB,1024,48,0,0},
  {"mulfix-1024",MULFIX,1024,24,0,0},
  {"divfix-1024",DIVFIX,1024,12,0,0},
  {"muldiv-1024",MULDIV,1024,12,0,0},
  {"muldiv-noround-1024",MULDIV_NO,1024,12,0,0},
  {"sqrt-1024",SQRT,1024,8,0,0},
  {"lcd-16",LCD,16,320,0,0},
  {"lcd-256",LCD,256,160,0,0},
  {"lcd-4096",LCD,4096,24,0,0},
  {"lcdv-16",LCD_V,16,320,0,0},
  {"lcdv-256",LCD_V,256,160,0,0},
  {"lcdv-4096",LCD_V,4096,24,0,0},
  {"lcdv-neg-256",LCD_V_NEG,256,160,0,0},
  {"lcdv-neg-4096",LCD_V_NEG,4096,24,0,0},
  {"blend-16",BLEND,16,200,0,0},
  {"blend-512",BLEND,512,36,0,0},
  {"blend-4096",BLEND,4096,10,0,0},
  {"blend-cold-16",BLEND_COLD,16,120,0,0},
  {"blend-cold-64",BLEND_COLD,64,120,0,0},
  {"blend-cold-256",BLEND_COLD,256,60,0,0},
  {"blend-cold-512",BLEND_COLD,512,36,0,0},
  {"blend-cold-4096",BLEND_COLD,4096,10,0,0},
  {"bgra-16",BGRA,16,512,0,0},
  {"bgra-64",BGRA,64,128,0,0},
  {"bgra-256",BGRA,256,32,0,0},
  {"bgra-512",BGRA,512,16,0,0},
  {"bgra-4096",BGRA,4096,8,0,0},
  {"bgra-opaque-16",BGRA,16,512,0,1},
  {"bgra-opaque-64",BGRA,64,128,0,1},
  {"bgra-opaque-256",BGRA,256,32,0,1},
  {"bgra-opaque-512",BGRA,512,16,0,1},
  {"bgra-opaque-4096",BGRA,4096,8,0,1},
  {"bgra-premul-16",BGRA,16,512,0,2},
  {"bgra-premul-64",BGRA,64,128,0,2},
  {"bgra-premul-256",BGRA,256,32,0,2},
  {"bgra-premul-512",BGRA,512,16,0,2},
  {"bgra-premul-4096",BGRA,4096,8,0,2},
  {"bgra-alpha1-16",BGRA,16,512,0,3},
  {"bgra-alpha1-64",BGRA,64,128,0,3},
  {"bgra-alpha1-256",BGRA,256,32,0,3},
  {"bgra-alpha1-512",BGRA,512,16,0,3},
  {"bgra-alpha1-4096",BGRA,4096,8,0,3},
  {"bgra-zero-16",BGRA,16,512,0,4},
  {"bgra-zero-64",BGRA,64,128,0,4},
  {"bgra-zero-256",BGRA,256,32,0,4},
  {"bgra-zero-512",BGRA,512,16,0,4},
  {"bgra-zero-4096",BGRA,4096,8,0,4},
  {"bgra-cold-16",BGRA_COLD,16,512,0,0},
  {"bgra-cold-64",BGRA_COLD,64,128,0,0},
  {"bgra-cold-256",BGRA_COLD,256,32,0,0},
  {"bgra-cold-512",BGRA_COLD,512,16,0,0},
  {"bgra-cold-4096",BGRA_COLD,4096,8,0,0},
  {"bgra-cold-opaque-16",BGRA_COLD,16,512,0,1},
  {"bgra-cold-opaque-64",BGRA_COLD,64,128,0,1},
  {"bgra-cold-opaque-256",BGRA_COLD,256,32,0,1},
  {"bgra-cold-opaque-512",BGRA_COLD,512,16,0,1},
  {"bgra-cold-opaque-4096",BGRA_COLD,4096,8,0,1},
  {"bgra-cold-premul-16",BGRA_COLD,16,512,0,2},
  {"bgra-cold-premul-64",BGRA_COLD,64,128,0,2},
  {"bgra-cold-premul-256",BGRA_COLD,256,32,0,2},
  {"bgra-cold-premul-512",BGRA_COLD,512,16,0,2},
  {"bgra-cold-premul-4096",BGRA_COLD,4096,8,0,2},
  {"bgra-cold-alpha1-16",BGRA_COLD,16,512,0,3},
  {"bgra-cold-alpha1-64",BGRA_COLD,64,128,0,3},
  {"bgra-cold-alpha1-256",BGRA_COLD,256,32,0,3},
  {"bgra-cold-alpha1-512",BGRA_COLD,512,16,0,3},
  {"bgra-cold-alpha1-4096",BGRA_COLD,4096,8,0,3},
  {"bgra-cold-zero-16",BGRA_COLD,16,512,0,4},
  {"bgra-cold-zero-64",BGRA_COLD,64,128,0,4},
  {"bgra-cold-zero-256",BGRA_COLD,256,32,0,4},
  {"bgra-cold-zero-512",BGRA_COLD,512,16,0,4},
  {"bgra-cold-zero-4096",BGRA_COLD,4096,8,0,4},
  {"mono-16",MONO,16,320,0,0},
  {"mono-512",MONO,512,80,0,0},
  {"mono-4096",MONO,4096,12,0,0},
  {"mono-cold-16",MONO_COLD,16,300,0,0},
  {"mono-cold-64",MONO_COLD,64,160,0,0},
  {"mono-cold-256",MONO_COLD,256,80,0,0},
  {"mono-cold-512",MONO_COLD,512,40,0,0},
  {"mono-cold-4096",MONO_COLD,4096,12,0,0},
  {"overlap-64",OVERLAP,64,256,0,0},
  {"overlap-4096",OVERLAP,4096,32,0,0},
  {"grayfill-8",GRAYFILL,8,300,0,0},
  {"grayfill-16",GRAYFILL,16,300,0,0},
  {"grayfill-31",GRAYFILL,31,300,0,0},
  {"grayfill-32",GRAYFILL,32,300,0,0},
  {"grayfill-63",GRAYFILL,63,300,0,0},
  {"grayfill-64",GRAYFILL,64,300,0,0},
  {"grayfill-128",GRAYFILL,128,300,0,0},
  {"grayfill-512",GRAYFILL,512,120,0,0},
  {"grayfill-4096",GRAYFILL,4096,16,0,0},
  {"bitmap-or-16",ROW_OR,16,400,0,0},
  {"bitmap-or-512",ROW_OR,512,100,0,0},
  {"bitmap-or-4096",ROW_OR,4096,16,0,0},
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
  {"pack-mono-8",PACK_MONO,8,220,0,0},
  {"pack-mono-16",PACK_MONO,16,220,0,0},
  {"pack-mono-31",PACK_MONO,31,220,0,0},
  {"pack-mono-32",PACK_MONO,32,220,0,0},
  {"pack-mono-64",PACK_MONO,64,220,0,0},
  {"pack-mono-128",PACK_MONO,128,220,0,0},
  {"pack-mono-256",PACK_MONO,256,220,0,0},
  {"pack-mono-512",PACK_MONO,512,70,0,0},
  {"pack-mono-4096",PACK_MONO,4096,12,0,0},
  {"pack-gray2-8",PACK_GRAY2,8,220,0,0},
  {"pack-gray2-16",PACK_GRAY2,16,220,0,0},
  {"pack-gray2-31",PACK_GRAY2,31,220,0,0},
  {"pack-gray2-32",PACK_GRAY2,32,220,0,0},
  {"pack-gray2-64",PACK_GRAY2,64,220,0,0},
  {"pack-gray2-128",PACK_GRAY2,128,220,0,0},
  {"pack-gray2-256",PACK_GRAY2,256,220,0,0},
  {"pack-gray2-512",PACK_GRAY2,512,70,0,0},
  {"pack-gray2-4096",PACK_GRAY2,4096,12,0,0},
  {"pack-gray4-8",PACK_GRAY4,8,220,0,0},
  {"pack-gray4-16",PACK_GRAY4,16,220,0,0},
  {"pack-gray4-31",PACK_GRAY4,31,220,0,0},
  {"pack-gray4-32",PACK_GRAY4,32,220,0,0},
  {"pack-gray4-64",PACK_GRAY4,64,220,0,0},
  {"pack-gray4-128",PACK_GRAY4,128,220,0,0},
  {"pack-gray4-256",PACK_GRAY4,256,220,0,0},
  {"pack-gray4-512",PACK_GRAY4,512,70,0,0},
  {"pack-gray4-4096",PACK_GRAY4,4096,12,0,0},
#endif
#if defined(RETRO_BENCH_R5900)
  {"embolden-gray8-8",EMBOLDEN_GRAY8,8,1024,0,0},
  {"embolden-gray8-16",EMBOLDEN_GRAY8,16,512,0,0},
  {"embolden-gray8-32",EMBOLDEN_GRAY8,32,256,0,0},
  {"embolden-gray8-64",EMBOLDEN_GRAY8,64,128,0,0},
  {"embolden-gray8-128",EMBOLDEN_GRAY8,128,64,0,0},
  {"embolden-gray8-256",EMBOLDEN_GRAY8,256,32,0,0},
  {"embolden-gray8-512",EMBOLDEN_GRAY8,512,16,0,0},
  {"embolden-gray8-4096",EMBOLDEN_GRAY8,4096,12,0,0},
  {"embolden-gray8-x2-8",EMBOLDEN_GRAY8_X2,8,1024,0,0},
  {"embolden-gray8-x2-16",EMBOLDEN_GRAY8_X2,16,512,0,0},
  {"embolden-gray8-x2-32",EMBOLDEN_GRAY8_X2,32,256,0,0},
  {"embolden-gray8-x2-64",EMBOLDEN_GRAY8_X2,64,128,0,0},
  {"embolden-gray8-x2-128",EMBOLDEN_GRAY8_X2,128,64,0,0},
  {"embolden-gray8-x2-256",EMBOLDEN_GRAY8_X2,256,32,0,0},
  {"embolden-gray8-x2-512",EMBOLDEN_GRAY8_X2,512,16,0,0},
  {"embolden-gray8-x2-4096",EMBOLDEN_GRAY8_X2,4096,12,0,0},
  {"embolden-gray8-x3-8",EMBOLDEN_GRAY8_X3,8,1024,0,0},
  {"embolden-gray8-x3-16",EMBOLDEN_GRAY8_X3,16,512,0,0},
  {"embolden-gray8-x3-32",EMBOLDEN_GRAY8_X3,32,256,0,0},
  {"embolden-gray8-x3-64",EMBOLDEN_GRAY8_X3,64,128,0,0},
  {"embolden-gray8-x3-128",EMBOLDEN_GRAY8_X3,128,64,0,0},
  {"embolden-gray8-x3-256",EMBOLDEN_GRAY8_X3,256,32,0,0},
  {"embolden-gray8-x3-512",EMBOLDEN_GRAY8_X3,512,16,0,0},
  {"embolden-gray8-x3-4096",EMBOLDEN_GRAY8_X3,4096,12,0,0},
  {"embolden-gray8-x4-8",EMBOLDEN_GRAY8_X4,8,1024,0,0},
  {"embolden-gray8-x4-16",EMBOLDEN_GRAY8_X4,16,512,0,0},
  {"embolden-gray8-x4-32",EMBOLDEN_GRAY8_X4,32,256,0,0},
  {"embolden-gray8-x4-64",EMBOLDEN_GRAY8_X4,64,128,0,0},
  {"embolden-gray8-x4-128",EMBOLDEN_GRAY8_X4,128,64,0,0},
  {"embolden-gray8-x4-256",EMBOLDEN_GRAY8_X4,256,32,0,0},
  {"embolden-gray8-x4-512",EMBOLDEN_GRAY8_X4,512,16,0,0},
  {"embolden-gray8-x4-4096",EMBOLDEN_GRAY8_X4,4096,12,0,0},
  {"embolden-gray8-unaligned1-256",EMBOLDEN_GRAY8,256,32,1,0},
  {"embolden-gray8-unaligned7-256",EMBOLDEN_GRAY8,256,32,7,0},
  {"embolden-gray8-unaligned15-256",EMBOLDEN_GRAY8,256,32,15,0},
  {"embolden-gray8-x2-unaligned1-256",EMBOLDEN_GRAY8_X2,256,32,1,0},
  {"embolden-gray8-x2-unaligned7-256",EMBOLDEN_GRAY8_X2,256,32,7,0},
  {"embolden-gray8-x2-unaligned15-256",EMBOLDEN_GRAY8_X2,256,32,15,0},
  {"embolden-gray8-x3-unaligned1-256",EMBOLDEN_GRAY8_X3,256,32,1,0},
  {"embolden-gray8-x3-unaligned7-256",EMBOLDEN_GRAY8_X3,256,32,7,0},
  {"embolden-gray8-x3-unaligned15-256",EMBOLDEN_GRAY8_X3,256,32,15,0},
  {"embolden-gray8-x4-unaligned1-256",EMBOLDEN_GRAY8_X4,256,32,1,0},
  {"embolden-gray8-x4-unaligned7-256",EMBOLDEN_GRAY8_X4,256,32,7,0},
  {"embolden-gray8-x4-unaligned15-256",EMBOLDEN_GRAY8_X4,256,32,15,0},
#endif
  {"grayfill-unaligned1-16",GRAYFILL,16,120,1,0},
  {"grayfill-unaligned1-64",GRAYFILL,64,120,1,0},
  {"grayfill-unaligned1-512",GRAYFILL,512,120,1,0},
  {"grayfill-unaligned1-4096",GRAYFILL,4096,120,1,0},
  {"grayfill-unaligned7-16",GRAYFILL,16,120,7,0},
  {"grayfill-unaligned7-64",GRAYFILL,64,120,7,0},
  {"grayfill-unaligned7-512",GRAYFILL,512,120,7,0},
  {"grayfill-unaligned7-4096",GRAYFILL,4096,120,7,0},
  {"grayfill-unaligned15-16",GRAYFILL,16,120,15,0},
  {"grayfill-unaligned15-64",GRAYFILL,64,120,15,0},
  {"grayfill-unaligned15-512",GRAYFILL,512,120,15,0},
  {"grayfill-unaligned15-4096",GRAYFILL,4096,120,15,0},
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
  {"pack-mono-unaligned1-512",PACK_MONO,512,70,1,0},
  {"pack-mono-unaligned7-512",PACK_MONO,512,70,7,0},
  {"pack-mono-unaligned15-512",PACK_MONO,512,70,15,0},
  {"pack-gray2-unaligned1-512",PACK_GRAY2,512,70,1,0},
  {"pack-gray2-unaligned7-512",PACK_GRAY2,512,70,7,0},
  {"pack-gray2-unaligned15-512",PACK_GRAY2,512,70,15,0},
  {"pack-gray4-unaligned1-512",PACK_GRAY4,512,70,1,0},
  {"pack-gray4-unaligned7-512",PACK_GRAY4,512,70,7,0},
  {"pack-gray4-unaligned15-512",PACK_GRAY4,512,70,15,0},
#endif

#if defined(RETRO_BENCH_R5900)
  {"embolden-gray8-low-32",EMBOLDEN_GRAY8,32,32,0,1},
  {"embolden-gray8-low-256",EMBOLDEN_GRAY8,256,32,0,1},
  {"embolden-gray8-low-4096",EMBOLDEN_GRAY8,4096,32,0,1},
  {"embolden-gray8-saturated-32",EMBOLDEN_GRAY8,32,32,0,2},
  {"embolden-gray8-saturated-256",EMBOLDEN_GRAY8,256,32,0,2},
  {"embolden-gray8-saturated-4096",EMBOLDEN_GRAY8,4096,32,0,2},
  {"embolden-gray8-x2-low-32",EMBOLDEN_GRAY8_X2,32,32,0,1},
  {"embolden-gray8-x2-low-256",EMBOLDEN_GRAY8_X2,256,32,0,1},
  {"embolden-gray8-x2-low-4096",EMBOLDEN_GRAY8_X2,4096,32,0,1},
  {"embolden-gray8-x2-saturated-32",EMBOLDEN_GRAY8_X2,32,32,0,2},
  {"embolden-gray8-x2-saturated-256",EMBOLDEN_GRAY8_X2,256,32,0,2},
  {"embolden-gray8-x2-saturated-4096",EMBOLDEN_GRAY8_X2,4096,32,0,2},
  {"embolden-gray8-x3-low-32",EMBOLDEN_GRAY8_X3,32,32,0,1},
  {"embolden-gray8-x3-low-256",EMBOLDEN_GRAY8_X3,256,32,0,1},
  {"embolden-gray8-x3-low-4096",EMBOLDEN_GRAY8_X3,4096,32,0,1},
  {"embolden-gray8-x3-saturated-32",EMBOLDEN_GRAY8_X3,32,32,0,2},
  {"embolden-gray8-x3-saturated-256",EMBOLDEN_GRAY8_X3,256,32,0,2},
  {"embolden-gray8-x3-saturated-4096",EMBOLDEN_GRAY8_X3,4096,32,0,2},
  {"embolden-gray8-x4-low-32",EMBOLDEN_GRAY8_X4,32,32,0,1},
  {"embolden-gray8-x4-low-256",EMBOLDEN_GRAY8_X4,256,32,0,1},
  {"embolden-gray8-x4-low-4096",EMBOLDEN_GRAY8_X4,4096,32,0,1},
  {"embolden-gray8-x4-saturated-32",EMBOLDEN_GRAY8_X4,32,32,0,2},
  {"embolden-gray8-x4-saturated-256",EMBOLDEN_GRAY8_X4,256,32,0,2},
  {"embolden-gray8-x4-saturated-4096",EMBOLDEN_GRAY8_X4,4096,32,0,2},
#endif

};
static const unsigned nc=(unsigned)(sizeof(cases)/sizeof(cases[0]));

/* The screen summary uses the variant names declared further down.
 * C99 requires an explicit declaration before the first call. */
static const char* label(int kind,unsigned v);

#if defined(RETRO_BENCH_R5900)
/*
 * PS2SDK's GS debug screen is independent of printf/PCSX2 stdout.
 * Keep all rendering OUTSIDE timed regions.  This screen is a brief
 * provisional summary; only verdict.py has the full paired bootstrap
 * correctness/noise criterion for production decisions.
 *
 * There are eighteen category lines (rows 3..20) plus a status/footer.
 * The largest or last available suite of each group is shown; ALL
 * individual input sizes and raw sample counts remain in RB1 logs.
 */
#define RB_SCREEN_GROUPS 18U
#define RB_WHITE 0x00ffffff
#define RB_GREEN 0x0000ff00
#define RB_RED   0x000000ff
#define RB_YELLOW 0x0000ffff

struct rb_screen_entry {
  const char* sample;
  const char* candidate;
  unsigned ratio_percent;
  int present;
};
static struct rb_screen_entry rb_screen_results[RB_SCREEN_GROUPS];
static const char* const rb_screen_names[RB_SCREEN_GROUPS]={
  "MSB", "MulFix", "DivFix", "MulDiv", "MulDiv NR",
  "SqrtFixed", "LCD H", "LCD V", "BGRA Blend",
  "BGRA Gray", "MONO Emb", "Overlap", "GRAY Fill",
  "Bitmap OR", "Pack MONO", "Pack GRAY2",
  "Pack GRAY4", "GRAY8 Emb x4"
};

/* All returns are literal group indices, never user data. */
static unsigned rb_screen_group(int kind)
{
  switch(kind){
  case MSB: return 0;
  case MULFIX: return 1;
  case DIVFIX: return 2;
  case MULDIV: return 3;
  case MULDIV_NO: return 4;
  case SQRT: return 5;
  case LCD: return 6;
  case LCD_V:case LCD_V_NEG: return 7;
  case BLEND:case BLEND_COLD: return 8;
  case BGRA:case BGRA_COLD: return 9;
  case MONO:case MONO_COLD: return 10;
  case OVERLAP:return 11;
  case GRAYFILL:return 12;
  case ROW_OR:return 13;
  case PACK_MONO:return 14;
  case PACK_GRAY2:return 15;
  case PACK_GRAY4:return 16;
  case EMBOLDEN_GRAY8:case EMBOLDEN_GRAY8_X2:
  case EMBOLDEN_GRAY8_X3:case EMBOLDEN_GRAY8_X4:return 17;
  }
  return RB_SCREEN_GROUPS;
}
static uint64_t rb_screen_median6(const uint64_t data[RB_SAMPLES])
{
  uint64_t tmp[RB_SAMPLES], v;
  unsigned i,j;

  for(i=0;i<RB_SAMPLES;i++)tmp[i]=data[i];
  for(i=1;i<RB_SAMPLES;i++){
    v=tmp[i];j=i;
    while(j>0 && tmp[j-1]>v){tmp[j]=tmp[j-1];j--;}
    tmp[j]=v;
  }
  return tmp[RB_SAMPLES/2-1]+
         (tmp[RB_SAMPLES/2]-tmp[RB_SAMPLES/2-1])/2;
}
static void rb_screen_start(void)
{
  unsigned i;
  init_scr(); /* initializes the GS framebuffer/display */
  scr_setCursor(0); /* disable visible cursor / end-of-line block */
  scr_setfontcolor(RB_WHITE);
  scr_setXY(0,0);
  scr_printf("FREETYPE RETRO | PS2 EE MMI | VALIDATION + BENCHMARK");
  scr_setXY(0,1);
  scr_printf("Scalar / A / B / MMI | %u suites | 6 paired samples",nc);
  scr_setXY(0,2);
  scr_printf("%-15s %-16s %s","FUNCTION","PROVISIONAL BEST","SPEED");
  for(i=0;i<RB_SCREEN_GROUPS;i++){
    scr_setXY(0,(int)i+3);
    scr_printf("%-15.15s %-16s %s",rb_screen_names[i],"WAIT","--");
  }
  scr_setXY(0,21);
  scr_printf("VERIFY   0/%-3u %-24s",nc,"starting");
}
static void rb_screen_progress(const char* phase,unsigned done,
                               const char* name)
{
  scr_setfontcolor(RB_WHITE);
  scr_setXY(0,21);
  scr_printf("%-11.11s %3u/%-3u %-24.24s       ",
             phase,done,nc,name);
}
/* Used for all exits on PS2, successful and unsuccessful.
 * SleepThread() intentionally never resumes without an interrupt.
 */
static int rb_screen_hold(int status,const char* phase,const char* name)
{
  scr_setXY(0,22);
  scr_setfontcolor(status?RB_RED:RB_GREEN);
  scr_printf("%s | %-14.14s %-21.21s  ",
             status?"RESULT: FAIL":"RESULT: PASS",phase,name);
  scr_setfontcolor(RB_WHITE);
  scr_setXY(0,23);
  scr_printf("Details: RB1 CHECK + SAMPLE records on stdout");
  scr_setXY(0,24);
  scr_printf("BUILD: %.36s",RETRO_BENCH_BUILD_ID);
  fflush(stdout);
  SleepThread();
  return status; /* unlikely to return */
}
static void rb_screen_record(const struct rb_case* t,
                              const uint64_t samples[4][RB_SAMPLES],
                              unsigned nvariants)
{
  unsigned g=rb_screen_group(t->kind),base=0,v,best=0;
  uint64_t med[4],baseline;
  unsigned percent;
  struct rb_screen_entry* dst;
  if(g>=RB_SCREEN_GROUPS)return;

  /* This is the representative largest normal-size case. Cold LUT
   * timings are still fully recorded, but do not replace it here. */
  if(t->kind==BLEND_COLD || t->kind==BGRA_COLD ||
     t->kind==MONO_COLD || t->offset || t->alpha_mode)return;

  if(t->kind==MSB && nvariants>1)base=1; /* FreeType builtin clz */
  if(t->kind==GRAYFILL && nvariants>1)base=1; /* libc memset */
  for(v=0;v<nvariants;v++)med[v]=rb_screen_median6(samples[v]);
  baseline=med[base];
  best=base;
  for(v=0;v<nvariants;v++){
    if(v==base || !med[v])continue;
    if(t->kind==MSB && v==0)continue; /* diagnostic bit scan */
    if(t->kind==MULFIX && v==1)continue; /* model, not MMI */
    if(t->kind==GRAYFILL && v==0)continue; /* diagnostic loop */
    if(t->kind==ROW_OR && v==1)continue; /* duplicate scalar control */
    if(med[v]<med[best])best=v;
  }
  percent=med[best]? (unsigned)((baseline*100U)/med[best]):100U;
  dst=&rb_screen_results[g];
  dst->present=1;
  dst->sample=t->name;
  /* A 5% gain is an on-screen provisional label ONLY. */
  dst->candidate=(best!=base && percent>=105U)?
                     label(t->kind,best):label(t->kind,base);
  dst->ratio_percent=(best!=base && percent>=105U)?percent:100U;
  /* Publish each measured group immediately; do not wait for final page. */
  scr_setXY(0,(int)g+3);
  scr_setfontcolor(dst->ratio_percent>100U?RB_GREEN:RB_WHITE);
  scr_printf("%-15.15s %-16.16s %3u.%02ux       ",
             rb_screen_names[g],dst->candidate,
             dst->ratio_percent/100U,dst->ratio_percent%100U);
}
static void rb_screen_results_page(void)
{
  unsigned i,shown=0;
  scr_setfontcolor(RB_WHITE);
  scr_setXY(0,0);
  scr_printf("FREETYPE RETRO | PS2 EE MMI | COMPLETE - PASS");
  scr_setXY(0,1);
  scr_printf("VALIDATION PASS | %u SUITES | 6 PAIRED SAMPLES",nc);
  scr_setXY(0,2);
  scr_printf("%-15s %-16s %s","FUNCTION","PROVISIONAL BEST","SPEED");
  for(i=0;i<RB_SCREEN_GROUPS;i++){
    const struct rb_screen_entry* x=&rb_screen_results[i];
    scr_setXY(0,(int)i+3);
    if(!x->present){
      scr_setfontcolor(RB_YELLOW);
      scr_printf("%-15.15s %-16s %s",rb_screen_names[i],"N/A","--");
      continue;
    }
    shown++;
    scr_setfontcolor(x->ratio_percent>100U?RB_GREEN:RB_WHITE);
    scr_printf("%-15.15s %-16.16s %3u.%02ux       ",
               rb_screen_names[i],x->candidate,
               x->ratio_percent/100U,x->ratio_percent%100U);
  }
  scr_setXY(0,21);
  scr_setfontcolor(RB_YELLOW);
  scr_printf("COMPLETE | measured groups %u/%u (largest cases)   ",shown,RB_SCREEN_GROUPS);
  scr_setXY(0,22);
  scr_setfontcolor(RB_GREEN);
  scr_printf("RESULT: PASS | checks + timings finished");
  scr_setXY(0,23);
  scr_setfontcolor(RB_WHITE);
  scr_printf("Winners provisional; confirm with verdict.py");
  scr_setXY(0,24);
  scr_printf("BUILD: %.36s",RETRO_BENCH_BUILD_ID);
}
#else
static void rb_screen_start(void) { }
static void rb_screen_progress(const char* phase,unsigned done,
                               const char* name)
{ (void)phase;(void)done;(void)name; }
static int rb_screen_hold(int status,const char* phase,const char* name)
{ (void)phase;(void)name;return status; }
static void rb_screen_record(const struct rb_case* t,
                              const uint64_t samples[4][RB_SAMPLES],
                              unsigned nvariants)
{ (void)t;(void)samples;(void)nvariants; }
static void rb_screen_results_page(void) { }
#endif

static uint32_t prng(uint32_t* state)
{
  uint32_t x=*state;
  x^=x<<13;x^=x>>17;x^=x<<5;
  return *state=x;
}
static uint64_t timestamp(void)
{
#if defined(RETRO_BENCH_R5900)
  return (uint64_t)GetTimerSystemTime();
#else
  struct timespec ts;
  if(clock_gettime(CLOCK_MONOTONIC,&ts)) { perror("clock_gettime"); exit(2); }
  return (uint64_t)ts.tv_sec*UINT64_C(1000000000)+ts.tv_nsec;
#endif
}
static uint64_t clock_hz(void)
{
#if defined(RETRO_BENCH_R5900)
  return (uint64_t)kBUSCLK;
#else
  return UINT64_C(1000000000);
#endif
}
static uint32_t checksum(const void* ptr,size_t len)
{
  const unsigned char* bytes=(const unsigned char*)ptr;
  uint32_t hash=UINT32_C(2166136261);
  size_t i;
  for(i=0;i<len;i++) { hash^=bytes[i]; hash*=UINT32_C(16777619); }
  return hash;
}
static const char* label(int kind,unsigned v)
{
  switch(kind) {
  case MSB:
    if(v==0)return "scalar";if(v==1)return "builtin";
    if(v==2)return "debruijn";
#if defined(RETRO_BENCH_R5900)
    if(v==3)return "plzcw";
#endif
    break;
  case MULFIX:
    if(v==0)return "scalar";if(v==1)return "hi_lo_words";
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
    if(v==2)return "target_hilo";
#if defined(RETRO_BENCH_R5900)
    if(v==3)return "scheduled_hilo";
#endif
#endif
    break;
  case DIVFIX: case MULDIV: case MULDIV_NO: case SQRT:
  case MONO: case MONO_COLD:
  case OVERLAP:
    if(v==0)return "scalar";if(v==1)return "option_c";
    break;
  case BGRA:case BGRA_COLD:
    if(v==0)return "scalar";if(v==1)return "option_c";
    if(v==2)return "reciprocal";
    break;
  case LCD:case LCD_V:case LCD_V_NEG:
    if(v==0)return "scalar";if(v==1)return "folded_c";
#if defined(RETRO_BENCH_R5900)
    if(v==2)return "mmi_paddb";
#endif
    break;
  case BLEND: case BLEND_COLD:
    if(v==0)return "scalar";if(v==1)return "exact255";
    if(v==2)return "lut";
    break;
  case GRAYFILL:
    if(v==0)return "scalar_loop";if(v==1)return "memset";
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
    if(v==2)return "target_span";
#if defined(RETRO_BENCH_R5900)
    if(v==3)return "scheduled_span";
#endif
#endif
    break;
  case ROW_OR:
    if(v==0)return "scalar";if(v==1)return "portable_loop";
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
    if(v==2)return "target_or";
#endif
    break;
  case PACK_MONO:case PACK_GRAY2:case PACK_GRAY4:
  case EMBOLDEN_GRAY8:case EMBOLDEN_GRAY8_X2:
  case EMBOLDEN_GRAY8_X3:case EMBOLDEN_GRAY8_X4:
    if(v==0)return "scalar";
#if defined(RETRO_BENCH_R5900)
    if(v==1)return "mmi_legacy";
    if(v==2)return "mmi";
    if(v==3)return "mmi_direct";
#elif defined(RETRO_BENCH_SPARC32)
    if(v==1)return "vis1";
#endif
    break;
  }
  return "unsupported";
}
static unsigned variants(int kind)
{
  if(kind==MSB)
#if defined(RETRO_BENCH_R5900)
    return 4;
#else
    return 3;
#endif
  if(kind==MULFIX)
#if defined(RETRO_BENCH_R5900)
    return 4;
#elif defined(RETRO_BENCH_SPARC32)
    return 3;
#else
    return 2;
#endif
  if(kind==LCD||kind==LCD_V||kind==LCD_V_NEG)
#if defined(RETRO_BENCH_R5900)
    return 3;
#else
    return 2;
#endif
  if(kind==BLEND||kind==BLEND_COLD||kind==BGRA||kind==BGRA_COLD)return 3;
#if defined(RETRO_BENCH_R5900)
  if(kind==PACK_MONO||kind==PACK_GRAY2||kind==GRAYFILL)return 4;
  if(kind==PACK_GRAY4||kind==EMBOLDEN_GRAY8||kind==EMBOLDEN_GRAY8_X2||
     kind==EMBOLDEN_GRAY8_X3||kind==EMBOLDEN_GRAY8_X4)return 3;
#endif
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
  if(kind==GRAYFILL||kind==ROW_OR)return 3;
#endif
  return 2;
}
static unsigned target_bytes(const struct rb_case* t)
{
  switch(t->kind) {
  case MSB:case MULFIX:case DIVFIX:case MULDIV:case MULDIV_NO:case SQRT:
    return t->size*4U;
  default: return RB_OUT_SIZE;
  }
}
/* Every mutable-workload candidate pays the same reset overhead,
 * but reset ONLY the region the kernel can touch. Otherwise a 16-byte
 * microbench would measure a 32K memcpy rather than its arithmetic.
 * The untouched guard suffix is initialized once per sample.
 */
static unsigned reset_bytes(const struct rb_case* t)
{
  const unsigned n=t->size;
  switch(t->kind){
  case LCD:return n*2U+64U;
  case LCD_V:case LCD_V_NEG:return n*5U+64U;
  case BLEND:case BLEND_COLD:return n*4U+64U;
  default:return n+64U;
  }
}
static void prepare(const struct rb_case* t,unsigned run_seed)
{
  uint32_t st=UINT32_C(0x73bd519b)^run_seed;
  unsigned i;
  (void)t;
  for(i=0;i<RB_ITEMS;i++) {
    uint32_t x=prng(&st),y=prng(&st),z=prng(&st);
    a[i]=x?x:1U;
    b[i]=y;
    /* A mix of power-two and ordinary nonzero divisors. */
    c[i]=(i%3==0)?(1U<<((i*7U)%31U)):
         ((i%7==0)?(x|1U):(z|1U));
    if(i%13==0)b[i]=c[i];
    if(i%17==0)a[i]=x&65535U;
    if(i%19==0)a[i]=0;
  }
  for(i=0;i<RB_OUT_SIZE;i++){
    input[i]=(FT_Byte)prng(&st);
    initial[i]=(FT_Byte)prng(&st);
  }
  if(t->kind==BGRA || t->kind==BGRA_COLD){
    for(i=0;i<t->size;i++){
      FT_Byte* px=input+4*i;
      if(t->alpha_mode==1)px[3]=255;
      if(t->alpha_mode==3)px[3]=1;
      if(t->alpha_mode==4)px[3]=0;
      if(t->alpha_mode==2){
        px[0]%=px[3]+1U;px[1]%=px[3]+1U;px[2]%=px[3]+1U;
      }
    }
  }
  if(t->kind>=EMBOLDEN_GRAY8 && t->alpha_mode){
    for(i=0;i<RB_OUT_SIZE;i++)initial[i]=t->alpha_mode==1?(initial[i]&15U):255;
  }
  /* All table setup is deliberately done OUTSIDE steady-state timing. */
  {
    FT_Color color={83,177,221,197};
    ft_bitmap_retro_blend_prepare(&blend_table,color);
  }
  ft_bitmap_retro_bgra_gray_prepare(&gray_table);
  ft_bitmap_retro_mono_embolden_prepare(&mono_table,4);
  memset(output,0xA5,sizeof(output));
  memset(result,0,sizeof(result));
  escape_sink=0;
}
static FT_UInt32 sqrt_original(FT_UInt32 v)
{
  uint64_t radicand;
  FT_UInt32 q,t;
  if(!v)return 0;
  radicand=((uint64_t)v<<16)-1U;
  q=1U<<((17U+FT_MSB(v))>>1);
  do{t=q;q=(t+(FT_UInt32)(radicand/t)+1U)>>1;}while(q!=t);
  return q;
}
static void lcd_original(FT_Byte* dst,unsigned width,unsigned char cover,
                         const unsigned char weights[5])
{
  unsigned i,k;
  for(i=0;i<width;i++)
    for(k=0;k<5;k++)
      dst[i+k]=(FT_Byte)(dst[i+k]+
                (((unsigned)cover*weights[k]+85U)>>8));
}
static void lcd_v_original(FT_Byte* dst,unsigned width,unsigned char cover,
                           const unsigned char weights[5],int pitch)
{
  unsigned i,k;
  for(k=0;k<5;k++){
    unsigned delta=((unsigned)cover*weights[k]+85U)>>8;
    for(i=0;i<width;i++)
      dst[i]=(FT_Byte)(dst[i]+delta);
    if(k<4)dst+=pitch;
  }
}
static void blend_original(FT_Byte* dst,const FT_Byte* mask,unsigned width,
                           FT_Color color)
{
  unsigned i;
  for(i=0;i<width;i++,dst+=4){
    unsigned fa=(color.alpha*mask[i])/255U;
    unsigned inv=255U-fa;
    unsigned fb=(color.blue*fa)/255U;
    unsigned fg=(color.green*fa)/255U;
    unsigned fr=(color.red*fa)/255U;
    dst[0]=(FT_Byte)(dst[0]*inv/255U+fb);
    dst[1]=(FT_Byte)(dst[1]*inv/255U+fg);
    dst[2]=(FT_Byte)(dst[2]*inv/255U+fr);
    dst[3]=(FT_Byte)(dst[3]*inv/255U+fa);
  }
}
static void bgra_original(FT_Byte* dst,const FT_Byte* src,unsigned width)
{
  unsigned i;
  for(i=0;i<width;i++,src+=4){
    unsigned alpha=src[3];
    uint32_t l=(4731UL*src[0]*src[0]+46868UL*src[1]*src[1]+
                13937UL*src[2]*src[2])>>16;
    dst[i]=alpha?(FT_Byte)(alpha-l/alpha):0;
  }
}
static void mono_original(FT_Byte* dst,unsigned width)
{
  int x;
  for(x=(int)width-1;x>=0;x--){
    unsigned i;FT_Byte raw=dst[x];
    for(i=1;i<=4;i++){
      dst[x]=(FT_Byte)(dst[x]|(raw>>i));
      if(x>0)dst[x]=(FT_Byte)(dst[x]|(dst[x-1]<<(8-i)));
    }
  }
}
static void overlap_original(FT_Byte* dst,unsigned x,unsigned count,unsigned cov)
{
  unsigned i;
  unsigned cover=(cov+8U)>>4;
  for(i=0;i<count;i++){
    unsigned at=(x+i)>>2;
    unsigned sum=(unsigned)dst[at]+cover;
    dst[at]=(FT_Byte)(sum-(sum>>8));
  }
}
static void kernel(const struct rb_case* t,unsigned v)
{
  unsigned i,n=t->size;
  const int kind=t->kind;
  FT_Color color={83,177,221,197};
  static const unsigned char weights[5]={8,77,86,77,8};
  if(kind>=LCD){
    memcpy(output,initial,reset_bytes(t)); /* identical touched-byte reset */
  }

  switch(kind){
  case MSB:
    for(i=0;i<n;i++){
      FT_UInt32 x=a[i] ? a[i]:1U;
      if(v==0)result[i]=rb_msb_scalar(x);
      else if(v==1){
#if defined(__GNUC__)
        result[i]=(FT_UInt32)(31-__builtin_clz(x));
#else
        result[i]=rb_msb_scalar(x);
#endif
      }else if(v==2)result[i]=(FT_UInt32)ft_msb_retro_sparc32(x);
#if defined(RETRO_BENCH_R5900)
      else result[i]=(FT_UInt32)rb_msb_hw(x);
#endif
    } break;
  case MULFIX:
    for(i=0;i<n;i++){
      if(v==0)result[i]=rb_mulfix_scalar((FT_Int32)a[i],(FT_Int32)b[i]);
      else if(v==1)result[i]=rb_mulfix_words((FT_Int32)a[i],(FT_Int32)b[i]);
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
      else result[i]=(FT_UInt32)
#if defined(RETRO_BENCH_R5900)
        (v==2?ft_mulfix_retro_hw_legacy((FT_Int32)a[i],(FT_Int32)b[i]):
              ft_mulfix_retro_hw((FT_Int32)a[i],(FT_Int32)b[i]));
#else
        ft_mulfix_retro_hw((FT_Int32)a[i],(FT_Int32)b[i]);
#endif
#endif
    } break;
  case DIVFIX:
    for(i=0;i<n;i++)result[i]=v?
      rb_divfix_quick((FT_Int32)a[i],(FT_Int32)c[i]):
      rb_divfix_scalar((FT_Int32)a[i],(FT_Int32)c[i]);
    break;
  case MULDIV:case MULDIV_NO:
    for(i=0;i<n;i++)result[i]=v?
      rb_muldiv_quick((FT_Int32)a[i],(FT_Int32)b[i],(FT_Int32)c[i],
                       kind==MULDIV):
      rb_muldiv_scalar((FT_Int32)a[i],(FT_Int32)b[i],(FT_Int32)c[i],
                        kind==MULDIV);
    break;
  case SQRT:
    for(i=0;i<n;i++)result[i]=v?ft_sqrt_retro_restoring(a[i]):
                                    sqrt_original(a[i]);
    break;
  case LCD:
    /* Three overlapping synthetic spans, identical coverage for each
     * implementation, including the extra right-side 4-tap border. */
    if(v==0) {
      lcd_original(output+rb_offset,n,143,weights);
      lcd_original(output+rb_offset+n/3,n/2,221,weights);
    } else if(v==1) {
      rb_lcd_c_horizontal(output+rb_offset,n,143,weights);
      rb_lcd_c_horizontal(output+rb_offset+n/3,n/2,221,weights);
    }
#if defined(RETRO_BENCH_R5900)
    else {
      rb_lcd_mmi_horizontal(output+rb_offset,n,143,weights);
      rb_lcd_mmi_horizontal(output+rb_offset+n/3,n/2,221,weights);
    }
#endif
    break;
  case LCD_V:case LCD_V_NEG:
    {
      int pitch=(int)n+8;
      FT_Byte* dst=output+rb_offset;
      if(kind==LCD_V_NEG){
        dst+=4*pitch;
        pitch=-pitch;
      }
      if(v==0)lcd_v_original(dst,n,197,weights,pitch);
      else if(v==1)rb_lcd_c_vertical(dst,n,197,weights,pitch);
#if defined(RETRO_BENCH_R5900)
      else rb_lcd_mmi_vertical(dst,n,197,weights,pitch);
#endif
    }
    break;
  case BLEND:case BLEND_COLD:
    if(v==2 && kind==BLEND_COLD)
      cold_blend_prepare(&blend_table,color);
    if(v==0)blend_original(output+rb_offset,input,n,color);
    else if(v==1)ft_bitmap_retro_blend_row(output+rb_offset,input,n,color);
    else ft_bitmap_retro_blend_row_lut(output+rb_offset,input,n,&blend_table);
    break;
  case BGRA:case BGRA_COLD:
    if(v && kind==BGRA_COLD)
      cold_gray_prepare(&gray_table);
    if(v==0)bgra_original(output+rb_offset,input,n);
    else if(v==1)ft_bitmap_retro_bgra_gray_row_div(output+rb_offset,input,n,&gray_table);
    else ft_bitmap_retro_bgra_gray_row(output+rb_offset,input,n,&gray_table);
    break;
  case MONO:case MONO_COLD:
    if(v && kind==MONO_COLD)
      cold_mono_prepare(&mono_table,4);
    if(v==0)mono_original(output+rb_offset,n);
    else ft_bitmap_retro_mono_embolden_row(output+rb_offset,(int)n,&mono_table);
    break;
  case OVERLAP:
    for(i=0;i<3;i++){
      unsigned x=16U+i*3U;
      unsigned cover=i==0?255U:i==1?193U:121U;
      if(v==0)overlap_original(output+rb_offset,x,n*2U,cover);
      else ft_smooth_retro_overlap_span(output+rb_offset,x,n*2U,cover);
    } break;
  case GRAYFILL:
    if(v==0)for(i=0;i<n;i++)output[rb_offset+i]=179;
    else if(v==1)memset(output+rb_offset,179,n);
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
    else
#if defined(RETRO_BENCH_R5900)
      if(v==2)ft_gray_retro_fill_legacy(output+rb_offset,179,(int)n);
      else
#endif
      ft_gray_retro_fill(output+rb_offset,179,(int)n);
#endif
    break;
  case ROW_OR:
    if(v<2)for(i=0;i<n;i++)output[rb_offset+i]|=input[rb_offset+i];
#if defined(RETRO_BENCH_R5900)
    else ft_bitmap_mmi_or_row(output+rb_offset,input+rb_offset,(FT_Int)n);
#elif defined(RETRO_BENCH_SPARC32)
    else ft_bitmap_vis1_or_row(output+rb_offset,input+rb_offset,(FT_Int)n);
#endif
    break;
  case PACK_MONO:
    if(v==0)for(i=0;i<n;i++)output[rb_offset+i]=(FT_Byte)(
      (input[rb_offset+(i>>3)]>>(7-(i&7U)))&1U);
#if defined(RETRO_BENCH_R5900)
    else if(v==1)ft_bitmap_mmi_convert_mono_row_legacy(input+rb_offset,output+rb_offset,n);
    else if(v==2)ft_bitmap_mmi_convert_mono_row(input+rb_offset,output+rb_offset,n);
    else ft_bitmap_mmi_convert_mono_row_direct(input+rb_offset,output+rb_offset,n);
#elif defined(RETRO_BENCH_SPARC32)
    else ft_bitmap_vis1_convert_mono_row(input+rb_offset,output+rb_offset,n);
#endif
    break;
  case PACK_GRAY2:
    if(v==0)for(i=0;i<n;i++)output[rb_offset+i]=(FT_Byte)(
      (input[rb_offset+(i>>2)]>>(6-2*(i&3U)))&3U);
#if defined(RETRO_BENCH_R5900)
    else if(v==1)ft_bitmap_mmi_convert_gray2_row_legacy(input+rb_offset,output+rb_offset,n);
    else if(v==2)ft_bitmap_mmi_convert_gray2_row(input+rb_offset,output+rb_offset,n);
    else ft_bitmap_mmi_convert_gray2_row_direct(input+rb_offset,output+rb_offset,n);
#elif defined(RETRO_BENCH_SPARC32)
    else ft_bitmap_vis1_convert_gray2_row(input+rb_offset,output+rb_offset,n);
#endif
    break;
  case PACK_GRAY4:
    if(v==0)for(i=0;i<n;i++)output[rb_offset+i]=(FT_Byte)(
      (input[rb_offset+(i>>1)]>>(4-4*(i&1U)))&15U);
#if defined(RETRO_BENCH_R5900)
    else if(v==1)ft_bitmap_mmi_convert_gray4_row_legacy(input+rb_offset,output+rb_offset,n);
    else ft_bitmap_mmi_convert_gray4_row(input+rb_offset,output+rb_offset,n);
#elif defined(RETRO_BENCH_SPARC32)
    else ft_bitmap_vis1_convert_gray4_row(input+rb_offset,output+rb_offset,n);
#endif
    break;
  case EMBOLDEN_GRAY8:case EMBOLDEN_GRAY8_X2:
  case EMBOLDEN_GRAY8_X3:case EMBOLDEN_GRAY8_X4:
    {
      unsigned strength=kind==EMBOLDEN_GRAY8?1U:
                        kind==EMBOLDEN_GRAY8_X2?2U:
                        kind==EMBOLDEN_GRAY8_X3?3U:4U;
      if(v==0){
        int x;
        for(x=(int)n-1;x>=0;x--){
          unsigned i;
          for(i=1;i<=strength && x>=(int)i;i++){
            unsigned sum=(unsigned)output[rb_offset+x]+output[rb_offset+x-i];
            output[rb_offset+x]=(FT_Byte)(sum>255U?255U:sum);
            if(sum>=255U)break;
          }
        }
      }
#if defined(RETRO_BENCH_R5900)
      else if(v==1)ft_bitmap_mmi_gray8_embolden_small_legacy(output+rb_offset,(FT_Int)n,
                                               (FT_Int)strength);
      else ft_bitmap_mmi_gray8_embolden_small(output+rb_offset,(FT_Int)n,
                                               (FT_Int)strength);
#endif
    }
    break;
  default:break;
  }
}
static unsigned digest(const struct rb_case* t)
{
  return (t->kind<LCD)?checksum(result,target_bytes(t)):
                        checksum(output,RB_OUT_SIZE);
}
static int run(const struct rb_case* t,unsigned v,unsigned reps)
{
  unsigned r;
  for(r=0;r<reps;r++){
    kernel(t,v);
    escape_sink+=(t->kind<LCD)?result[(r*7U)%t->size]:
                         output[rb_offset+(r*7U)%t->size];
  }
  return 1;
}
static int validate_case(const struct rb_case* t,int log_checks)
{
  unsigned v,trial,n=variants(t->kind);
  unsigned saved_offset=rb_offset;
  FT_Byte oracle[RB_OUT_SIZE];
  FT_UInt32 ref[RB_ITEMS];
  unsigned expected,got;
  for(trial=0;trial<5;trial++){
    { static const unsigned offsets[5]={0,1,7,8,15};
      rb_offset=16U+((t->offset+offsets[trial])&15U); }
    prepare(t,UINT32_C(0x6f234c91)+(unsigned)trial*101U);
    if(!run(t,0,1))return 0;
    expected=digest(t);
    memcpy(oracle,output,sizeof(oracle));
    memcpy(ref,result,sizeof(ref));
    if(log_checks)
      printf("RB1,CHECK,%s,%s,PASS,%08lx\n",t->name,
             label(t->kind,0),(unsigned long)expected);
    for(v=1;v<n;v++){
      prepare(t,UINT32_C(0x6f234c91)+(unsigned)trial*101U);
      if(!run(t,v,1))return 0;
      got=digest(t);
      if(got!=expected||
         (t->kind<LCD?memcmp(ref,result,t->size*4U):
                      memcmp(oracle,output,RB_OUT_SIZE))){
        fprintf(stderr,"RB1,CHECK,%s,%s,FAIL,%08lx\n",
                t->name,label(t->kind,v),(unsigned long)got);
        return 0;
      }
      if(log_checks)
        printf("RB1,CHECK,%s,%s,PASS,%08lx\n",t->name,
               label(t->kind,v),(unsigned long)got);
    }
  }
  rb_offset=saved_offset;
  return 1;
}
int main(void)
{
  unsigned j,s,step,v,n;
  uint64_t elapsed,hz=clock_hz(),begin,end;
  const struct rb_case* t;

  /* PS2: actually initialize the GS debug framebuffer before any
   * validation or timing. Even failures display a persistent screen.
   * Host/SPARC builds have no-op screen functions. */
  rb_screen_start();
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
  if(sizeof(void*)!=4 || sizeof(long)!=4){
    fprintf(stderr,"RB1,FATAL,abi,expected-32-bit-long-and-pointers\n");
    return rb_screen_hold(2,"ABI ERROR","32-bit toolchain required");
  }
#endif
  printf("RB1,META,%s,%s,%u,%llu\n",
         RB_TARGET,RETRO_BENCH_BUILD_ID,RB_SAMPLES,
         (unsigned long long)hz);

  /* Declare the complete workload/variant manifest before testing.
   * verdict.py requires exactly five CHECK and six SAMPLE rows for
   * every declared variant, and GATE/DONE must match nc.  A captured
   * log missing an entire suite must never produce a recommendation.
   */
  for(j=0;j<nc;j++){
    unsigned k;
    const struct rb_case* entry=&cases[j];

    printf("RB1,CASE,%s,%u,%u",entry->name,entry->reps,entry->size);
    for(k=0;k<variants(entry->kind);k++)
      printf(",%s",label(entry->kind,k));
    putchar('\n');
  }
  fflush(stdout);
  /* Full buffer correctness and guard checks BEFORE any timings. */
  for(j=0;j<nc;j++){
    rb_screen_progress("VERIFY",j+1,cases[j].name);
    if(!validate_case(&cases[j],1)){
      fprintf(stderr,"RB1,FATAL,%s,correctness\n",cases[j].name);
      return rb_screen_hold(1,"CHECK FAILED",cases[j].name);
    }
  }
  printf("RB1,GATE,PASS,%u\n",nc);
  fflush(stdout);
  rb_offset=16U; /* force aligned timing on real VIS1/MMI targets */
  for(j=0;j<nc;j++){
    uint64_t samples[4][RB_SAMPLES]={{0}};
    t=&cases[j];
    n=variants(t->kind);
    rb_screen_progress("CHECK+BENCH",j+1,t->name);
    /* The global RB1 correctness gate remains before every SAMPLE.
     * Recheck this workload immediately before its timings without
     * adding CHECK rows after GATE (verdict.py requires strict order). */
    if(!validate_case(t,0)){
      fprintf(stderr,"RB1,FATAL,%s,pre-benchmark-correctness\n",t->name);
      return rb_screen_hold(1,"CHECK FAILED",t->name);
    }
    /* Select the DECLARED timed alignment after checking all correctness
     * offsets. Validation also preserves caller state as a regression
     * safeguard against silently timing scalar conversion fallbacks.
     */
    rb_offset=16U+t->offset;
    /* AB/BA and ABC/BCA/CAB; stable input, rotated thermal/cache order */
    for(s=0;s<RB_SAMPLES;s++){
      uint32_t sample_digests[4]={0,0,0,0};
      for(step=0;step<n;step++){
        v=(step+s)%n;
        prepare(t,UINT32_C(0x7d5192ab));
        begin=timestamp();
        run(t,v,t->reps);
        end=timestamp();
        elapsed=end-begin;
        if(!elapsed){
          fprintf(stderr,"RB1,FATAL,%s,zero-ticks\n",t->name);
          return rb_screen_hold(2,"TIMER FAILED",t->name);
        }
        samples[v][s]=elapsed;
        sample_digests[v]=digest(t);
        /* Timer is already stopped before hashing or I/O. */
        printf("RB1,SAMPLE,%s,%s,%u,%u,%llu,%llu,%08lx\n",
               t->name,label(t->kind,v),s,t->reps,
               (unsigned long long)elapsed,(unsigned long long)hz,
               (unsigned long)sample_digests[v]);
        fflush(stdout);
      }
      for(v=1;v<n;v++)
        if(sample_digests[v]!=sample_digests[0]){
          fprintf(stderr,"RB1,FATAL,%s,timed-digest-mismatch\n",t->name);
          return rb_screen_hold(1,"RESULT MISMATCH",t->name);
        }
    }
    /* Post-timer median for immediate on-screen provisional results.
     * Robust paired CI/noise decisions remain in verdict.py on host. */
    rb_screen_record(t,samples,n);
  }
  printf("RB1,DONE,PASS,%u\n",nc);
  fflush(stdout);
  rb_screen_results_page();
  return rb_screen_hold(0,"ALL CHECKS OK","see RB1 log for sizes");
}
