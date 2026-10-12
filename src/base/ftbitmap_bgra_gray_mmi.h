/****************************************************************************
 *
 * ftbitmap_bgra_gray_mmi.h
 *
 *   Optional PS2 R5900 MMI BGRA to GRAY row conversion.
 *   Included internally by ftbitmap.c, not a public API.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_BGRA_GRAY_MMI_H_
#define FTBITMAP_BGRA_GRAY_MMI_H_

#if !defined( __mips__ ) || !defined( __GNUC__ )
#error "FT_CONFIG_OPTION_MMI_BGRA_GRAY requires R5900/GCC MMI support"
#endif

/*
 * The scalar conversion is
 *
 *   l = ( 4731*B*B + 46868*G*G + 13937*R*R ) >> 16;
 *   output = A ? A - l / A : 0;
 *
 * Four pixels at a time, B*B, G*G and R*R come from PMULTH.  With
 * y = x*x - 32768 and the green weight reduced by 65536 the three weights
 * sum to zero, so
 *
 *   l = ( ( 4731*yb - 18668*yg + 13937*yr ) >>a 16 ) + g*g
 *
 * exactly, and every operand fits a signed halfword for PMULTH/PMADDH.
 * The division by A uses a reciprocal that is exact for 0 <= l <= 65025.
 * No lookup tables are built per call.
 */

static const FT_Byte ft_bgra_gray_mmi_c[5][16] __attribute__((aligned(16))) = {
  {0xff,0,0xff,0,0xff,0,0xff,0,0xff,0,0xff,0,0xff,0,0xff,0}, /* mask */
  {0,0x80,0,0,0,0x80,0,0,0,0x80,0,0,0,0x80,0,0},              /* ^0x8000 */
  {0x7b,0x12,0x7b,0x12,0x7b,0x12,0x7b,0x12,0x7b,0x12,0x7b,0x12,0x7b,0x12,0x7b,0x12}, /* 4731 */
  {0x14,0xb7,0x14,0xb7,0x14,0xb7,0x14,0xb7,0x14,0xb7,0x14,0xb7,0x14,0xb7,0x14,0xb7}, /* 46868 */
  {0x71,0x36,0x71,0x36,0x71,0x36,0x71,0x36,0x71,0x36,0x71,0x36,0x71,0x36,0x71,0x36}  /* 13937 */
};


/* One pixel, exact: squares by multiplication, reciprocal division. */
static void
ft_bitmap_mmi_bgra_gray_tail( FT_Byte*        dst,
                              const FT_Byte*  src,
                              FT_UInt         width )
{
  FT_UInt  i;


  for ( i = 0; i < width; i++, src += 4 )
  {
    FT_UInt  a = src[3];


    if ( !a )
      dst[i] = 0;
    else
    {
      FT_UInt32  b = src[0], g = src[1], r = src[2];
      FT_UInt32  l = ( 4731U * ( b * b ) + 46868U * ( g * g ) +
                       13937U * ( r * r ) ) >> 16;


      dst[i] = (FT_Byte)( a - ft_bitmap_retro_gray_divide( l, a ) );
    }
  }
}

typedef struct { FT_UInt32 w[4]; } ft_bgra_gray_mmi_v4;
static void
ft_bitmap_mmi_bgra_gray_quad(FT_Byte* dst,const FT_Byte* src,FT_UInt width)
{
  const FT_UInt32* rec=ft_bgra_gray_reciprocal;
  while(width && ((FT_ULong)src&15UL))
  {
    ft_bitmap_mmi_bgra_gray_tail(dst,src,1);
    dst++; src+=4; width--;
  }
  for(; width>=4; width-=4,src+=16,dst+=4)
  {
    ft_bgra_gray_mmi_v4 lv __attribute__((aligned(16)));
    /* sq needs a 16B-aligned slot */
    FT_ULong v,m,x,br,ga,rr,sb,sg,sr,t;
    FT_UInt a0=src[3],a1=src[7],a2=src[11],a3=src[15];
    FT_UInt32 l0,l1,l2,l3,r0=rec[a0],r1=rec[a1],r2=rec[a2],r3=rec[a3];
    FT_UInt q0,q1,q2,q3;
    __asm__ (
      "lq %[v], 0(%[s])\n\t"
      "lq %[m], 0(%[c])\n\t"
      "pand %[br], %[v], %[m]\n\t"
      "psrlw %[ga], %[v], 8\n\t"
      "psrlw %[rr], %[br], 16\n\t"
      "pand %[ga], %[ga], %[m]\n\t"
      "lq %[x], 16(%[c])\n\t"
      "pmulth %[sb], %[br], %[br]\n\t"
      "pmulth %[sg], %[ga], %[ga]\n\t"
      "pmulth %[sr], %[rr], %[rr]\n\t"
      "pxor %[br], %[sb], %[x]\n\t"
      "pxor %[ga], %[sg], %[x]\n\t"
      "pxor %[rr], %[sr], %[x]\n\t"
      "lq %[v], 32(%[c])\n\t"
      "lq %[m], 48(%[c])\n\t"
      "lq %[x], 64(%[c])\n\t"
      "pmulth %[t], %[br], %[v]\n\t"
      "pmaddh %[t], %[ga], %[m]\n\t"
      "pmaddh %[t], %[rr], %[x]\n\t"
      "psraw %[t], %[t], 16\n\t"
      "paddw %[t], %[t], %[sg]\n\t"
      "sq %[t], %[out]\n\t"
      : [out]"=m"(lv), [v]"=&r"(v),[m]"=&r"(m),[x]"=&r"(x),[br]"=&r"(br),
        [ga]"=&r"(ga),[rr]"=&r"(rr),[sb]"=&r"(sb),[sg]"=&r"(sg),
        [sr]"=&r"(sr),[t]"=&r"(t)
      : [s]"r"(src),[c]"r"(ft_bgra_gray_mmi_c),"m"(*(const ft_bgra_gray_mmi_v4*)src)
      : "memory");
    l0=lv.w[0]; l1=lv.w[1]; l2=lv.w[2]; l3=lv.w[3];
    q0=(l0*r0)>>16; q1=(l1*r1)>>16; q2=(l2*r2)>>16; q3=(l3*r3)>>16;
    dst[0]=(FT_Byte)(a0-q0+(q0*a0>l0));
    dst[1]=(FT_Byte)(a1-q1+(q1*a1>l1));
    dst[2]=(FT_Byte)(a2-q2+(q2*a2>l2));
    dst[3]=(FT_Byte)(a3-q3+(q3*a3>l3));
  }
  if(width)ft_bitmap_mmi_bgra_gray_tail(dst,src,width);
}

/* m[a]=ceil(2^31/a), m[0]=0: floor(l/a)==hi32((2*l)*m[a]) for 0<=l<=65025 */
static const FT_UInt32 ft_bgra_gray_m31[256] __attribute__((aligned(64))) = {
  0U, 2147483648U, 1073741824U, 715827883U, 536870912U, 429496730U, 357913942U, 306783379U,
  268435456U, 238609295U, 214748365U, 195225787U, 178956971U, 165191050U, 153391690U, 143165577U,
  134217728U, 126322568U, 119304648U, 113025456U, 107374183U, 102261127U, 97612894U, 93368855U,
  89478486U, 85899346U, 82595525U, 79536432U, 76695845U, 74051161U, 71582789U, 69273667U,
  67108864U, 65075263U, 63161284U, 61356676U, 59652324U, 58040099U, 56512728U, 55063684U,
  53687092U, 52377650U, 51130564U, 49941481U, 48806447U, 47721859U, 46684428U, 45691142U,
  44739243U, 43826197U, 42949673U, 42107523U, 41297763U, 40518560U, 39768216U, 39045158U,
  38347923U, 37675152U, 37025581U, 36398028U, 35791395U, 35204650U, 34636834U, 34087043U,
  33554432U, 33038210U, 32537632U, 32051995U, 31580642U, 31122952U, 30678338U, 30246249U,
  29826162U, 29417585U, 29020050U, 28633116U, 28256364U, 27889399U, 27531842U, 27183338U,
  26843546U, 26512144U, 26188825U, 25873297U, 25565282U, 25264514U, 24970741U, 24683721U,
  24403224U, 24129030U, 23860930U, 23598722U, 23342214U, 23091223U, 22845571U, 22605092U,
  22369622U, 22139007U, 21913099U, 21691755U, 21474837U, 21262215U, 21053762U, 20849356U,
  20648882U, 20452226U, 20259280U, 20069941U, 19884108U, 19701685U, 19522579U, 19346700U,
  19173962U, 19004281U, 18837576U, 18673771U, 18512791U, 18354562U, 18199014U, 18046082U,
  17895698U, 17747799U, 17602325U, 17459217U, 17318417U, 17179870U, 17043522U, 16909321U,
  16777216U, 16647161U, 16519105U, 16393005U, 16268816U, 16146494U, 16025998U, 15907287U,
  15790321U, 15675064U, 15561476U, 15449523U, 15339169U, 15230381U, 15123125U, 15017369U,
  14913081U, 14810233U, 14708793U, 14608733U, 14510025U, 14412642U, 14316558U, 14221747U,
  14128182U, 14035841U, 13944700U, 13854734U, 13765921U, 13678240U, 13591669U, 13506187U,
  13421773U, 13338408U, 13256072U, 13174747U, 13094413U, 13015053U, 12936649U, 12859184U,
  12782641U, 12707004U, 12632257U, 12558384U, 12485371U, 12413201U, 12341861U, 12271336U,
  12201612U, 12132676U, 12064515U, 11997116U, 11930465U, 11864551U, 11799361U, 11734884U,
  11671107U, 11608020U, 11545612U, 11483870U, 11422786U, 11362348U, 11302546U, 11243370U,
  11184811U, 11126859U, 11069504U, 11012737U, 10956550U, 10900933U, 10845878U, 10791376U,
  10737419U, 10683999U, 10631108U, 10578738U, 10526881U, 10475530U, 10424678U, 10374318U,
  10324441U, 10275042U, 10226113U, 10177648U, 10129640U, 10082083U, 10034971U, 9988297U,
  9942054U, 9896239U, 9850843U, 9805862U, 9761290U, 9717121U, 9673350U, 9629972U,
  9586981U, 9544372U, 9502141U, 9460281U, 9418788U, 9377658U, 9336886U, 9296467U,
  9256396U, 9216669U, 9177281U, 9138229U, 9099507U, 9061113U, 9023041U, 8985288U,
  8947849U, 8910721U, 8873900U, 8837382U, 8801163U, 8765240U, 8729609U, 8694266U,
  8659209U, 8624433U, 8589935U, 8555712U, 8521761U, 8488078U, 8454661U, 8421505U,
};

static void
ft_bitmap_mmi_bgra_gray_row(FT_Byte* dst,const FT_Byte* src,FT_UInt width)
{
  while(width && ((FT_ULong)src&15UL))
  {
    ft_bitmap_mmi_bgra_gray_tail(dst,src,1);
    dst++; src+=4; width--;
  }
  for(; width>=16; width-=16,src+=64,dst+=16)
  {
    FT_UInt32 lv[16] __attribute__((aligned(16)));
    FT_ULong v,m,x,cb,cg,cr,br,ga,rr,sb,sg,sr,t;
    FT_UInt32 *lp=lv;
    __asm__ (
      "lq %[m], 0(%[c])\n\t"
      "lq %[x], 16(%[c])\n\t"
      "lq %[cb], 32(%[c])\n\t"
      "lq %[cg], 48(%[c])\n\t"
      "lq %[cr], 64(%[c])\n\t"
      "lq %[v], 0(%[s])\n\t"
      "pand %[br], %[v], %[m]\n\t"
      "psrlw %[ga], %[v], 8\n\t"
      "psrlw %[rr], %[br], 16\n\t"
      "pand %[ga], %[ga], %[m]\n\t"
      "pmulth %[sb], %[br], %[br]\n\t"
      "pmulth %[sg], %[ga], %[ga]\n\t"
      "pmulth %[sr], %[rr], %[rr]\n\t"
      "pxor %[br], %[sb], %[x]\n\t"
      "pxor %[ga], %[sg], %[x]\n\t"
      "pxor %[rr], %[sr], %[x]\n\t"
      "pmulth %[t], %[br], %[cb]\n\t"
      "pmaddh %[t], %[ga], %[cg]\n\t"
      "pmaddh %[t], %[rr], %[cr]\n\t"
      "psraw %[t], %[t], 16\n\t"
      "paddw %[t], %[t], %[sg]\n\t"
      "paddw %[t], %[t], %[t]\n\t"
      "sq %[t], 0(%[o])\n\t"
      "lq %[v], 16(%[s])\n\t"
      "pand %[br], %[v], %[m]\n\t"
      "psrlw %[ga], %[v], 8\n\t"
      "psrlw %[rr], %[br], 16\n\t"
      "pand %[ga], %[ga], %[m]\n\t"
      "pmulth %[sb], %[br], %[br]\n\t"
      "pmulth %[sg], %[ga], %[ga]\n\t"
      "pmulth %[sr], %[rr], %[rr]\n\t"
      "pxor %[br], %[sb], %[x]\n\t"
      "pxor %[ga], %[sg], %[x]\n\t"
      "pxor %[rr], %[sr], %[x]\n\t"
      "pmulth %[t], %[br], %[cb]\n\t"
      "pmaddh %[t], %[ga], %[cg]\n\t"
      "pmaddh %[t], %[rr], %[cr]\n\t"
      "psraw %[t], %[t], 16\n\t"
      "paddw %[t], %[t], %[sg]\n\t"
      "paddw %[t], %[t], %[t]\n\t"
      "sq %[t], 16(%[o])\n\t"
      "lq %[v], 32(%[s])\n\t"
      "pand %[br], %[v], %[m]\n\t"
      "psrlw %[ga], %[v], 8\n\t"
      "psrlw %[rr], %[br], 16\n\t"
      "pand %[ga], %[ga], %[m]\n\t"
      "pmulth %[sb], %[br], %[br]\n\t"
      "pmulth %[sg], %[ga], %[ga]\n\t"
      "pmulth %[sr], %[rr], %[rr]\n\t"
      "pxor %[br], %[sb], %[x]\n\t"
      "pxor %[ga], %[sg], %[x]\n\t"
      "pxor %[rr], %[sr], %[x]\n\t"
      "pmulth %[t], %[br], %[cb]\n\t"
      "pmaddh %[t], %[ga], %[cg]\n\t"
      "pmaddh %[t], %[rr], %[cr]\n\t"
      "psraw %[t], %[t], 16\n\t"
      "paddw %[t], %[t], %[sg]\n\t"
      "paddw %[t], %[t], %[t]\n\t"
      "sq %[t], 32(%[o])\n\t"
      "lq %[v], 48(%[s])\n\t"
      "pand %[br], %[v], %[m]\n\t"
      "psrlw %[ga], %[v], 8\n\t"
      "psrlw %[rr], %[br], 16\n\t"
      "pand %[ga], %[ga], %[m]\n\t"
      "pmulth %[sb], %[br], %[br]\n\t"
      "pmulth %[sg], %[ga], %[ga]\n\t"
      "pmulth %[sr], %[rr], %[rr]\n\t"
      "pxor %[br], %[sb], %[x]\n\t"
      "pxor %[ga], %[sg], %[x]\n\t"
      "pxor %[rr], %[sr], %[x]\n\t"
      "pmulth %[t], %[br], %[cb]\n\t"
      "pmaddh %[t], %[ga], %[cg]\n\t"
      "pmaddh %[t], %[rr], %[cr]\n\t"
      "psraw %[t], %[t], 16\n\t"
      "paddw %[t], %[t], %[sg]\n\t"
      "paddw %[t], %[t], %[t]\n\t"
      "sq %[t], 48(%[o])\n\t"
      : "=m"(*(struct{FT_UInt32 w[16];}*)lv),
        [v]"=&r"(v),[m]"=&r"(m),[x]"=&r"(x),[cb]"=&r"(cb),[cg]"=&r"(cg),
        [cr]"=&r"(cr),[br]"=&r"(br),[ga]"=&r"(ga),[rr]"=&r"(rr),
        [sb]"=&r"(sb),[sg]"=&r"(sg),[sr]"=&r"(sr),[t]"=&r"(t)
      : [s]"r"(src),[o]"r"(lp),[c]"r"(ft_bgra_gray_mmi_c),
        "m"(*(const struct{FT_Byte b[64];}*)src)
      : "memory");
#define FT_BGRA_GRAY_MMI_TAIL(K) { FT_UInt a=src[4*(K)+3];       dst[K]=(FT_Byte)(a-(FT_UInt32)(((unsigned long long)lv[K]*ft_bgra_gray_m31[a])>>32)); }
    FT_BGRA_GRAY_MMI_TAIL(0) FT_BGRA_GRAY_MMI_TAIL(1) FT_BGRA_GRAY_MMI_TAIL(2) FT_BGRA_GRAY_MMI_TAIL(3)
    FT_BGRA_GRAY_MMI_TAIL(4) FT_BGRA_GRAY_MMI_TAIL(5) FT_BGRA_GRAY_MMI_TAIL(6) FT_BGRA_GRAY_MMI_TAIL(7)
    FT_BGRA_GRAY_MMI_TAIL(8) FT_BGRA_GRAY_MMI_TAIL(9) FT_BGRA_GRAY_MMI_TAIL(10) FT_BGRA_GRAY_MMI_TAIL(11)
    FT_BGRA_GRAY_MMI_TAIL(12) FT_BGRA_GRAY_MMI_TAIL(13) FT_BGRA_GRAY_MMI_TAIL(14) FT_BGRA_GRAY_MMI_TAIL(15)
#undef FT_BGRA_GRAY_MMI_TAIL
  }
  if(width)ft_bitmap_mmi_bgra_gray_quad(dst,src,width);
}

#endif /* FTBITMAP_BGRA_GRAY_MMI_H_ */
