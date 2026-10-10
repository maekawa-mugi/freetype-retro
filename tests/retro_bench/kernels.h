/*
 * FreeType retro benchmark - ACTUAL source-tree helper kernels.
 * Inspired by openssl-retro test/ps2's isolated A/B/F structure.
 *
 * Minimal scalar typedefs let these static helper headers compile
 * standalone, without linking the full FreeType library. Keep the
 * end-to-end tests separate: they validate the integration points.
 */
#ifndef RETRO_BENCH_KERNELS_H
#define RETRO_BENCH_KERNELS_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef uint8_t FT_Byte;
typedef uint32_t FT_UInt32;
typedef uint16_t FT_UInt16;
typedef int32_t FT_Int32;
typedef uint64_t FT_UInt64;
typedef unsigned int FT_UInt;
typedef int FT_Int;
typedef int FT_Bool;
typedef unsigned long FT_ULong;
typedef struct {
  FT_Byte blue, green, red, alpha;
} FT_Color;

#define FT_SIZEOF_INT 4
#define FT_SIZEOF_LONG 4
#define FT_INT64 1

static FT_UInt rb_msb_scalar(FT_UInt32 v)
{
  FT_UInt bit;
  for (bit=31;bit>0;--bit)
    if(v & (UINT32_C(1)<<bit)) return bit;
  return 0;
}
static FT_UInt rb_msb_builtin(FT_UInt32 v)
{
#if defined(__GNUC__)
  return v ? (FT_UInt)(31 - __builtin_clz(v)) : 0;
#else
  return rb_msb_scalar(v);
#endif
}
#define FT_MSB(v) rb_msb_builtin(v)

#define FT_RETRO_MULDIV_MODEL_ONLY
#define FT_RETRO_DIVFIX_MODEL_ONLY
#define FT_RETRO_SQRT_MODEL_ONLY
#define FT_RETRO_MSB_MODEL_ONLY
#define FT_CONFIG_OPTION_RETRO_MSB_SPARC32
#define FT_CONFIG_OPTION_RETRO_BLEND_LUT
#include "../../src/base/ftmuldiv_retro.h"
#include "../../src/base/ftdivfix_retro.h"
#include "../../src/base/ftsqrtrestro.h"
#include "../../include/freetype/internal/ftmsb_retro.h"
#include "../../src/base/ftbitmap_bgra_gray_retro.h"
#include "../../src/base/ftbitmap_mono_embolden_retro.h"
#include "../../src/base/ftbitmap_blend_retro.h"
#include "../../src/smooth/ftsmooth_retro_overlap.h"

/* Two differently named inclusions: C and real MMI share the
 * same source, and coexist in a single PS2 ELF. No model substitution.
 */
#define ft_smooth_retro_lcd_add_bytes rb_lcd_c_add
#define ft_smooth_retro_lcd_horizontal rb_lcd_c_horizontal
#define ft_smooth_retro_lcd_vertical rb_lcd_c_vertical
#include "../../src/smooth/ftsmooth_retro_lcd.h"
#undef ft_smooth_retro_lcd_add_bytes
#undef ft_smooth_retro_lcd_horizontal
#undef ft_smooth_retro_lcd_vertical

#if defined(RETRO_BENCH_R5900)
#undef FTSMOOTH_RETRO_LCD_H_
#define FT_CONFIG_OPTION_MMI_LCD_SPANS
#define ft_smooth_retro_lcd_add_bytes rb_lcd_mmi_add
#define ft_smooth_retro_lcd_horizontal rb_lcd_mmi_horizontal
#define ft_smooth_retro_lcd_vertical rb_lcd_mmi_vertical
#include "../../src/smooth/ftsmooth_retro_lcd.h"
#undef ft_smooth_retro_lcd_add_bytes
#undef ft_smooth_retro_lcd_horizontal
#undef ft_smooth_retro_lcd_vertical

#undef FT_CONFIG_OPTION_RETRO_MSB_SPARC32
#undef FT_RETRO_MSB_MODEL_ONLY
#undef FTMSB_RETRO_H_
#undef FT_RETRO_MSB_FUNC
#define FT_CONFIG_OPTION_RETRO_MSB_R5900
#define ft_msb_retro_r5900 rb_msb_hw
#include "../../include/freetype/internal/ftmsb_retro.h"
#undef ft_msb_retro_r5900
#define FT_CONFIG_OPTION_RETRO_MULFIX_R5900
#elif defined(RETRO_BENCH_SPARC32)
#define FT_CONFIG_OPTION_RETRO_MULFIX_SPARC32
#else
#define FT_RETRO_MULFIX_MODEL_ONLY
#endif

#include "../../include/freetype/internal/ftmulfix_retro.h"

/* Target bitmap OR and long grayscale span backends, also compared in
 * the same executable against plain scalar C and libc memset.
 */
#if defined(RETRO_BENCH_R5900)
#include "../../src/base/ftbitmap_mmi.h"
#include "../../src/base/ftbitmap_convert_mmi.h"
#define FT_CONFIG_OPTION_MMI_GRAY_SPANS
#define FT_MEM_SET(p,val,n) memset((p),(val),(n))
#include "../../src/smooth/ftgrays_retro.h"
#elif defined(RETRO_BENCH_SPARC32)
#include "../../src/base/ftbitmap_vis1.h"
#include "../../src/base/ftbitmap_convert_vis1.h"
#define FT_CONFIG_OPTION_VIS1_GRAY_SPANS
#define FT_MEM_SET(p,val,n) memset((p),(val),(n))
#include "../../src/smooth/ftgrays_retro.h"
#endif

static FT_UInt32 rb_magnitude(FT_Int32 n)
{
  return n<0 ? 0U-(FT_UInt32)n:(FT_UInt32)n;
}
static FT_UInt32 rb_divfix_scalar(FT_Int32 a,FT_Int32 b)
{
  FT_UInt32 x=rb_magnitude(a),y=rb_magnitude(b),q;
  if(!y) q=UINT32_C(0x7fffffff);
  else q=(FT_UInt32)(((uint64_t)x*65536U+(y>>1))/y);
  return ((a<0)!=(b<0)) ? 0U-q:q;
}
static FT_UInt32 rb_divfix_quick(FT_Int32 a,FT_Int32 b)
{
  FT_UInt32 x=rb_magnitude(a),y=rb_magnitude(b),q;
  if(!ft_divfix_retro_fast32(x,y,&q))
    return rb_divfix_scalar(a,b);
  return ((a<0)!=(b<0)) ? 0U-q:q;
}
static FT_UInt32 rb_muldiv_scalar(FT_Int32 a,FT_Int32 b,FT_Int32 c,int round)
{
  FT_UInt32 x=rb_magnitude(a),y=rb_magnitude(b),z=rb_magnitude(c),q;
  if(!z) q=UINT32_C(0x7fffffff);
  else q=(FT_UInt32)(((uint64_t)x*y+(round?(z>>1):0U))/z);
  return ((a<0)^(b<0)^(c<0)) ? 0U-q:q;
}
static FT_UInt32 rb_muldiv_quick(FT_Int32 a,FT_Int32 b,FT_Int32 c,int round)
{
  FT_UInt32 x=rb_magnitude(a),y=rb_magnitude(b),z=rb_magnitude(c),q;
  if(!ft_muldiv_retro_fast32(x,y,z,round,&q))
    return rb_muldiv_scalar(a,b,c,round);
  return ((a<0)^(b<0)^(c<0)) ? 0U-q:q;
}
static FT_UInt32 rb_mulfix_scalar(FT_Int32 a,FT_Int32 b)
{
  int64_t p=(int64_t)a*b;
  return (FT_UInt32)((p+(p<0?32767:32768))>>16);
}
static FT_UInt32 rb_mulfix_words(FT_Int32 a,FT_Int32 b)
{
  uint64_t p=(uint64_t)((int64_t)a*b);
  return (FT_UInt32)ft_mulfix_retro_round_words(
    (FT_UInt32)p,(FT_Int32)(p>>32));
}
#endif
