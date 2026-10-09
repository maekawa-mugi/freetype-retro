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
static FT_Byte input[RB_OUT_SIZE],initial[RB_OUT_SIZE],output[RB_OUT_SIZE];
static FT_Retro_Blend_LUT blend_table;
static FT_Retro_BGRA_Gray_Table gray_table;
static FT_Retro_Mono_Embolden_Table mono_table;
static volatile FT_UInt32 escape_sink;

enum { MSB,MULFIX,DIVFIX,MULDIV,MULDIV_NO,SQRT,LCD,BLEND,BLEND_COLD,
       BGRA,BGRA_COLD,MONO,MONO_COLD,OVERLAP,GRAYFILL,ROW_OR };
struct rb_case { const char* name; int kind; unsigned size; unsigned reps; };
static const struct rb_case cases[] = {
  {"msb-1024",MSB,1024,48},
  {"mulfix-1024",MULFIX,1024,24},
  {"divfix-1024",DIVFIX,1024,12},
  {"muldiv-1024",MULDIV,1024,12},
  {"muldiv-noround-1024",MULDIV_NO,1024,12},
  {"sqrt-1024",SQRT,1024,8},
  {"lcd-16",LCD,16,320},
  {"lcd-256",LCD,256,160},
  {"lcd-4096",LCD,4096,24},
  {"blend-16",BLEND,16,200},
  {"blend-512",BLEND,512,36},
  {"blend-4096",BLEND,4096,10},
  {"blend-cold-16",BLEND_COLD,16,120},
  {"blend-cold-4096",BLEND_COLD,4096,10},
  {"bgra-16",BGRA,16,500},
  {"bgra-512",BGRA,512,120},
  {"bgra-4096",BGRA,4096,16},
  {"bgra-cold-16",BGRA_COLD,16,320},
  {"bgra-cold-4096",BGRA_COLD,4096,16},
  {"mono-16",MONO,16,320},
  {"mono-512",MONO,512,80},
  {"mono-4096",MONO,4096,12},
  {"mono-cold-16",MONO_COLD,16,300},
  {"mono-cold-4096",MONO_COLD,4096,12},
  {"overlap-64",OVERLAP,64,256},
  {"overlap-4096",OVERLAP,4096,32},
  {"grayfill-16",GRAYFILL,16,300},
  {"grayfill-512",GRAYFILL,512,120},
  {"grayfill-4096",GRAYFILL,4096,16},
  {"bitmap-or-16",ROW_OR,16,400},
  {"bitmap-or-512",ROW_OR,512,100},
  {"bitmap-or-4096",ROW_OR,4096,16}
};
static const unsigned nc=(unsigned)(sizeof(cases)/sizeof(cases[0]));

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
#endif
    break;
  case DIVFIX: case MULDIV: case MULDIV_NO: case SQRT:
  case BGRA: case BGRA_COLD: case MONO: case MONO_COLD:
  case OVERLAP:
    if(v==0)return "scalar";if(v==1)return "option_c";
    break;
  case LCD:
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
#endif
    break;
  case ROW_OR:
    if(v==0)return "scalar";if(v==1)return "portable_loop";
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
    if(v==2)return "target_or";
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
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
    return 3;
#else
    return 2;
#endif
  if(kind==LCD)
#if defined(RETRO_BENCH_R5900)
    return 3;
#else
    return 2;
#endif
  if(kind==BLEND||kind==BLEND_COLD)return 3;
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
  q=1U<<((17U+rb_msb_scalar(v))>>1);
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
    memcpy(output,initial,RB_OUT_SIZE); /* identical reset per operation */
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
      else result[i]=(FT_UInt32)ft_mulfix_retro_hw((FT_Int32)a[i],
                                                    (FT_Int32)b[i]);
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
      lcd_original(output+16,n,143,weights);
      lcd_original(output+16+n/3,n/2,221,weights);
    } else if(v==1) {
      rb_lcd_c_horizontal(output+16,n,143,weights);
      rb_lcd_c_horizontal(output+16+n/3,n/2,221,weights);
    }
#if defined(RETRO_BENCH_R5900)
    else {
      rb_lcd_mmi_horizontal(output+16,n,143,weights);
      rb_lcd_mmi_horizontal(output+16+n/3,n/2,221,weights);
    }
#endif
    break;
  case BLEND:case BLEND_COLD:
    if(v==2 && kind==BLEND_COLD)
      ft_bitmap_retro_blend_prepare(&blend_table,color);
    if(v==0)blend_original(output+16,input,n,color);
    else if(v==1)ft_bitmap_retro_blend_row(output+16,input,n,color);
    else ft_bitmap_retro_blend_row_lut(output+16,input,n,&blend_table);
    break;
  case BGRA:case BGRA_COLD:
    if(v && kind==BGRA_COLD)
      ft_bitmap_retro_bgra_gray_prepare(&gray_table);
    if(v==0)bgra_original(output+16,input,n);
    else ft_bitmap_retro_bgra_gray_row(output+16,input,n,&gray_table);
    break;
  case MONO:case MONO_COLD:
    if(v && kind==MONO_COLD)
      ft_bitmap_retro_mono_embolden_prepare(&mono_table,4);
    if(v==0)mono_original(output+16,n);
    else ft_bitmap_retro_mono_embolden_row(output+16,(int)n,&mono_table);
    break;
  case OVERLAP:
    for(i=0;i<3;i++){
      unsigned x=16U+i*3U;
      unsigned cover=i==0?255U:i==1?193U:121U;
      if(v==0)overlap_original(output+16,x,n*2U,cover);
      else ft_smooth_retro_overlap_span(output+16,x,n*2U,cover);
    } break;
  case GRAYFILL:
    if(v==0)for(i=0;i<n;i++)output[16+i]=179;
    else if(v==1)memset(output+16,179,n);
#if defined(RETRO_BENCH_R5900) || defined(RETRO_BENCH_SPARC32)
    else ft_gray_retro_fill(output+16,179,(int)n);
#endif
    break;
  case ROW_OR:
    if(v<2)for(i=0;i<n;i++)output[16+i]|=input[16+i];
#if defined(RETRO_BENCH_R5900)
    else ft_bitmap_mmi_or_row(output+16,input+16,(FT_Int)n);
#elif defined(RETRO_BENCH_SPARC32)
    else ft_bitmap_vis1_or_row(output+16,input+16,(FT_Int)n);
#endif
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
                         output[16U+(r*7U)%t->size];
  }
  return 1;
}
static int validate_case(const struct rb_case* t)
{
  unsigned v,trial,n=variants(t->kind);
  FT_Byte oracle[RB_OUT_SIZE];
  FT_UInt32 ref[RB_ITEMS];
  unsigned expected,got;
  for(trial=0;trial<5;trial++){
    prepare(t,UINT32_C(0x6f234c91)+(unsigned)trial*101U);
    if(!run(t,0,1))return 0;
    expected=digest(t);
    memcpy(oracle,output,sizeof(oracle));
    memcpy(ref,result,sizeof(ref));
    printf("RB1,CHECK,%s,scalar,PASS,%08lx\n",t->name,(unsigned long)expected);
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
      printf("RB1,CHECK,%s,%s,PASS,%08lx\n",t->name,
             label(t->kind,v),(unsigned long)got);
    }
  }
  return 1;
}
int main(void)
{
  unsigned j,s,step,v,n;
  uint64_t elapsed,hz=clock_hz(),begin,end;
  const struct rb_case* t;

  printf("RB1,META,%s,%s,%u,%llu\n",
         RB_TARGET,RETRO_BENCH_BUILD_ID,RB_SAMPLES,
         (unsigned long long)hz);
  fflush(stdout);
  /* Full buffer correctness and guard checks BEFORE any timings. */
  for(j=0;j<nc;j++)
    if(!validate_case(&cases[j])){
      fprintf(stderr,"RB1,FATAL,%s,correctness\n",cases[j].name);
      return 1;
    }
  printf("RB1,GATE,PASS,%u\n",nc);
  fflush(stdout);
  for(j=0;j<nc;j++){
    t=&cases[j];
    n=variants(t->kind);
    /* AB/BA and ABC/BCA/CAB; stable input, rotated thermal/cache order */
    for(s=0;s<RB_SAMPLES;s++)for(step=0;step<n;step++){
      v=(step+s)%n;
      prepare(t,UINT32_C(0x7d5192ab));
      begin=timestamp();
      run(t,v,t->reps);
      end=timestamp();
      elapsed=end-begin;
      if(!elapsed){
        fprintf(stderr,"RB1,FATAL,%s,zero-ticks\n",t->name);
        return 2;
      }
      printf("RB1,SAMPLE,%s,%s,%u,%u,%llu,%llu,%08lx\n",
             t->name,label(t->kind,v),s,t->reps,
             (unsigned long long)elapsed,(unsigned long long)hz,
             (unsigned long)digest(t));
      fflush(stdout);
    }
  }
  printf("RB1,DONE,PASS,%u\n",nc);
  return 0;
}
