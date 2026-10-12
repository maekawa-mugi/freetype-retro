/****************************************************************************
 *
 * ftbitmap_blend_mmi.h
 *
 *   Optional PS2 R5900 MMI FT_Bitmap_Blend row kernel.
 *   Included internally by ftbitmap.c, not a public API.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_BLEND_MMI_H_
#define FTBITMAP_BLEND_MMI_H_

#if !defined( __mips__ ) || !defined( __GNUC__ )
#error "FT_CONFIG_OPTION_MMI_BLEND requires R5900/GCC MMI support"
#endif

/*
 * Four pixels per quadword.  The 4 KiB table must be 16-byte aligned.
 * dst * inverse comes from PMULTH + PMFHL.LH (the low 16 bits of a
 * product of at most 65025), and the exact division by 255 is
 * ( x + 1 + ( x >> 8 ) ) >> 8 on halfwords.
 */

/* Per coverage value: four copies of the inverse opacity, then the four */
/* premultiplied source channels (B, G, R, A), all as halfwords.          */
typedef struct  FT_MMI_Blend_Table_
{
  FT_UInt16  e[256][8];

} FT_MMI_Blend_Table;
typedef FT_UInt32 __attribute__((may_alias)) ft_u32_alias;
static const FT_UInt16 ft_mmi_blend_one[8] __attribute__((aligned(16))) = {1,1,1,1,1,1,1,1};

static void
ft_bitmap_mmi_blend_prepare( FT_MMI_Blend_Table*  t,
                     FT_Color      color )
{
  FT_UInt  aa;

  for ( aa = 0; aa < 256; ++aa )
  {
    FT_UInt  fa  = ft_bitmap_retro_div255( (FT_UInt)color.alpha * aa );
    FT_UInt  inv = 255U - fa;

    FT_UInt32  w0 = inv * 0x00010001U;
    FT_UInt32  fb = ft_bitmap_retro_div255( (FT_UInt)color.blue  * fa );
    FT_UInt32  fg = ft_bitmap_retro_div255( (FT_UInt)color.green * fa );
    FT_UInt32  fr = ft_bitmap_retro_div255( (FT_UInt)color.red   * fa );
    ft_u32_alias*  w  = (ft_u32_alias*)t->e[aa];

    w[0] = w0;
    w[1] = w0;
    w[2] = fb | ( fg << 16 );
    w[3] = fr | ( fa << 16 );
  }
}

static void
ft_bitmap_mmi_blend_row( FT_Byte*              dst,
                   const FT_Byte*        coverage,
                   FT_UInt               width,
                   const FT_MMI_Blend_Table*    t )
{
  while ( width && ( (FT_ULong)dst & 15UL ) )
  {
    const FT_UInt16*  e = t->e[*coverage++];
    FT_UInt           c;

    for ( c = 0; c < 4; c++ )
      dst[c] = (FT_Byte)( ft_bitmap_retro_div255( dst[c] * e[0] ) + e[4 + c] );
    dst += 4;
    width--;
  }
  for ( ; width >= 4; width -= 4, coverage += 4, dst += 16 )
  {
    const FT_Byte*  e0 = (const FT_Byte*)t->e[coverage[0]];
    const FT_Byte*  e1 = (const FT_Byte*)t->e[coverage[1]];
    const FT_Byte*  e2 = (const FT_Byte*)t->e[coverage[2]];
    const FT_Byte*  e3 = (const FT_Byte*)t->e[coverage[3]];
    FT_ULong        d, a0, a1, a2, a3, i01, f01, i23, f23, dl, dh, p0, p1, q, c1;


    __asm__ volatile (
      "lq %[d], 0(%[dst])\n\t"
      "lq %[a0], 0(%[e0])\n\t"
      "lq %[a1], 0(%[e1])\n\t"
      "lq %[a2], 0(%[e2])\n\t"
      "lq %[a3], 0(%[e3])\n\t"
      "lq %[c1], 0(%[one])\n\t"
      "pextlb %[dl], $zero, %[d]\n\t"
      "pextub %[dh], $zero, %[d]\n\t"
      "pcpyld %[i01], %[a1], %[a0]\n\t"
      "pcpyud %[f01], %[a0], %[a1]\n\t"
      "pcpyld %[i23], %[a3], %[a2]\n\t"
      "pcpyud %[f23], %[a2], %[a3]\n\t"
      "pmulth %[q], %[dl], %[i01]\n\t"
      "pmfhl.lh %[p0]\n\t"
      "pmulth %[q], %[dh], %[i23]\n\t"
      "pmfhl.lh %[p1]\n\t"
      "psrlh %[q], %[p0], 8\n\t"
      "paddh %[p0], %[p0], %[q]\n\t"
      "psrlh %[q], %[p1], 8\n\t"
      "paddh %[p1], %[p1], %[q]\n\t"
      "paddh %[p0], %[p0], %[c1]\n\t"
      "paddh %[p1], %[p1], %[c1]\n\t"
      "psrlh %[p0], %[p0], 8\n\t"
      "psrlh %[p1], %[p1], 8\n\t"
      "paddh %[p0], %[p0], %[f01]\n\t"
      "paddh %[p1], %[p1], %[f23]\n\t"
      "ppacb %[p0], %[p1], %[p0]\n\t"
      "sq %[p0], 0(%[dst])\n\t"
      : [d]"=&r"(d),[a0]"=&r"(a0),[a1]"=&r"(a1),[a2]"=&r"(a2),[a3]"=&r"(a3),
        [i01]"=&r"(i01),[f01]"=&r"(f01),[i23]"=&r"(i23),[f23]"=&r"(f23),
        [dl]"=&r"(dl),[dh]"=&r"(dh),[p0]"=&r"(p0),[p1]"=&r"(p1),[q]"=&r"(q),
        [c1]"=&r"(c1)
      : [dst]"r"(dst),[e0]"r"(e0),[e1]"r"(e1),[e2]"r"(e2),[e3]"r"(e3),
        [one]"r"(ft_mmi_blend_one)
      : "memory","hi","lo" );
  }
  while ( width-- )
  {
    const FT_UInt16*  e = t->e[*coverage++];
    FT_UInt           c;

    for ( c = 0; c < 4; c++ )
      dst[c] = (FT_Byte)( ft_bitmap_retro_div255( dst[c] * e[0] ) + e[4 + c] );
    dst += 4;
  }
}

#endif /* FTBITMAP_BLEND_MMI_H_ */
