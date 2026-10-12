/****************************************************************************
 *
 * ftbitmap_convert_mmi_ext.h
 *
 *   Table-free MMI packed-bitmap expansion for the PS2 R5900:
 *   GRAY2 by shift folding (64 pixels per quadword load) and MONO by an
 *   interleave tree.  Aligned bulk paths; everything else falls back to
 *   the existing MMI row routines.  Output is byte-identical.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_CONVERT_MMI_EXT_H_
#define FTBITMAP_CONVERT_MMI_EXT_H_

static const FT_Byte ft_mmi_gray2_mask[16] __attribute__((aligned(16))) = {
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3
};

/* Fold the four terms into two stages: u=b|(b<<20), then
 * (u>>6)|(u<<4) gives shifts -6,14,4,24 before the final mask.
 * Two fewer MMI instructions per four source bytes than iteration 6.
 */
static void
ft_bitmap_mmi_convert_gray2_row_fold(const FT_Byte* src, FT_Byte* dst, FT_UInt width)
{
  FT_UInt blocks=width>>4, x=0;
  if(blocks && !((FT_ULong)src&3UL) && !((FT_ULong)dst&15UL))
  {
    const FT_Byte* sp=src;
    FT_Byte* dp=dst;
    FT_ULong word,out,tmp,mask;
    x=blocks<<4;
    __asm__ volatile (
      ".set push\n\t" ".set noreorder\n\t"
      "lq %5, 0(%7)\n\t"
      "1:\n\t"
      "lwu %2, 0(%0)\n\t"
      "pextlb %2, $zero, %2\n\t"
      "pextlh %2, $zero, %2\n\t"
      "psllw %4, %2, 20\n\t"
      "por %2, %2, %4\n\t"
      "psrlw %3, %2, 6\n\t"
      "psllw %4, %2, 4\n\t"
      "por %3, %3, %4\n\t"
      "pand %3, %3, %5\n\t"
      "sq %3, 0(%1)\n\t"
      "addiu %0, %0, 4\n\t"
      "addiu %6, %6, -1\n\t"
      "bne %6, $zero, 1b\n\t"
      "addiu %1, %1, 16\n\t"
      ".set pop\n\t"
      : "+r"(sp), "+r"(dp), "=&r"(word), "=&r"(out),
        "=&r"(tmp), "=&r"(mask), "+r"(blocks)
      : "r"(ft_mmi_gray2_mask) : "memory");
  }
  ft_bitmap_mmi_convert_gray2_row_legacy(src+x/4,dst+x,width-x);
}

#define FT_MMI_X_GRAY2_FOLD_VEC(V,OFF) \
  "psllw %8, " #V ", 20\n\t" \
  "por " #V ", " #V ", %8\n\t" \
  "psrlw %7, " #V ", 6\n\t" \
  "psllw %8, " #V ", 4\n\t" \
  "por %7, %7, %8\n\t" \
  "pand %7, %7, %9\n\t" \
  "sq %7, " #OFF "(%1)\n\t"

/* One LQ, 64 output pixels, four independent word vectors.
 * The 16-pixel fold candidate handles aligned tails; legacy handles
 * all remaining nonaligned and short rows.
 */
static void
ft_bitmap_mmi_convert_gray2_row_fold64(const FT_Byte* src, FT_Byte* dst, FT_UInt width)
{
  FT_UInt blocks=width>>6, x=0;
  if(blocks && !((FT_ULong)src&15UL) && !((FT_ULong)dst&15UL))
  {
    const FT_Byte* sp=src;
    FT_Byte* dp=dst;
    FT_ULong s,lo,hi,v0,v1,out,tmp,mask;
    x=blocks<<6;
    __asm__ volatile (
      ".set push\n\t" ".set noreorder\n\t"
      "lq %9, 0(%11)\n\t"
      "1:\n\t"
      "lq %2, 0(%0)\n\t"
      "pextlb %3, $zero, %2\n\t"
      "pextub %4, $zero, %2\n\t"
      "pextlh %5, $zero, %3\n\t"
      "pextuh %6, $zero, %3\n\t"
      "pextlh %3, $zero, %4\n\t"
      "pextuh %4, $zero, %4\n\t"
      FT_MMI_X_GRAY2_FOLD_VEC(%5,0)
      FT_MMI_X_GRAY2_FOLD_VEC(%6,16)
      FT_MMI_X_GRAY2_FOLD_VEC(%3,32)
      FT_MMI_X_GRAY2_FOLD_VEC(%4,48)
      "addiu %0, %0, 16\n\t"
      "addiu %10, %10, -1\n\t"
      "bne %10, $zero, 1b\n\t"
      "addiu %1, %1, 64\n\t"
      ".set pop\n\t"
      : "+r"(sp), "+r"(dp), "=&r"(s), "=&r"(lo), "=&r"(hi),
        "=&r"(v0), "=&r"(v1), "=&r"(out), "=&r"(tmp),
        "=&r"(mask), "+r"(blocks)
      : "r"(ft_mmi_gray2_mask) : "memory");
  }
  ft_bitmap_mmi_convert_gray2_row_fold(src+x/4,dst+x,width-x);
}
#undef FT_MMI_X_GRAY2_FOLD_VEC

static const FT_Byte ft_mmi_bitmask16[16] __attribute__(( aligned( 16 ) )) = {
  0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
  0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};
static const FT_Byte ft_mmi_one16[16] __attribute__(( aligned( 16 ) )) = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

#define FT_MMI_X_MONO_VEC( V, OFF )                    \
  "pand    %10, " #V ", %8\n\t"                   \
  "pceqb   %10, %10, %8\n\t"                      \
  "pand    %10, %10, %9\n\t"                      \
  "sq      %10, " #OFF "(%1)\n\t"

static void
ft_bitmap_mmi_convert_mono_row_tree( const FT_Byte*  src,
                    FT_Byte*        dst,
                    FT_UInt         width )
{
  FT_UInt  x = 0;


  if ( !( (FT_ULong)src & 15UL ) && !( (FT_ULong)dst & 15UL ) )
  {
    FT_UInt   blocks = width >> 7;       /* 128 pixels per block */
    FT_ULong  s, a, b, v0, v1, v2, m, one, t;
    const FT_Byte*  sp = src;
    FT_Byte*        dp = dst;


    if ( blocks )
    {
      x = blocks << 7;
      __asm__ volatile (
        ".set push\n\t" ".set noreorder\n\t"
        "lq      %8, 0(%12)\n\t"
        "lq      %9, 0(%13)\n\t"
        "1:\n\t"
        "lq      %2, 0(%0)\n\t"
        "pextlb  %3, %2, %2\n\t"
        "pextub  %4, %2, %2\n\t"
        "pextlh  %5, %3, %3\n\t"
        "pextuh  %6, %3, %3\n\t"
        "pextlh  %7, %4, %4\n\t"
        "pextuh  %4, %4, %4\n\t"
        /* %5=A0 %6=A1 %7=B0 %4=B1 ; each expands to two vectors */
        "pextlw  %2, %5, %5\n\t"
        FT_MMI_X_MONO_VEC( %2, 0 )
        "pextuw  %2, %5, %5\n\t"
        FT_MMI_X_MONO_VEC( %2, 16 )
        "pextlw  %2, %6, %6\n\t"
        FT_MMI_X_MONO_VEC( %2, 32 )
        "pextuw  %2, %6, %6\n\t"
        FT_MMI_X_MONO_VEC( %2, 48 )
        "pextlw  %2, %7, %7\n\t"
        FT_MMI_X_MONO_VEC( %2, 64 )
        "pextuw  %2, %7, %7\n\t"
        FT_MMI_X_MONO_VEC( %2, 80 )
        "pextlw  %2, %4, %4\n\t"
        FT_MMI_X_MONO_VEC( %2, 96 )
        "pextuw  %2, %4, %4\n\t"
        FT_MMI_X_MONO_VEC( %2, 112 )
        "addiu   %0, %0, 16\n\t"
        "addiu   %11, %11, -1\n\t"
        "bgtz    %11, 1b\n\t"
        "addiu   %1, %1, 128\n\t"
        ".set pop\n\t"
        : "+r" ( sp ), "+r" ( dp ), "=&r" ( s ), "=&r" ( a ), "=&r" ( b ),
          "=&r" ( v0 ), "=&r" ( v1 ), "=&r" ( v2 ), "=&r" ( m ),
          "=&r" ( one ), "=&r" ( t ), "+r" ( blocks )
        : "r" ( ft_mmi_bitmask16 ), "r" ( ft_mmi_one16 )
        : "memory" );
    }
  }

  if ( x < width )
    ft_bitmap_mmi_convert_mono_row( src + ( x >> 3 ), dst + x, width - x );
}
#undef FT_MMI_X_MONO_VEC

#endif /* FTBITMAP_CONVERT_MMI_EXT_H_ */
