/****************************************************************************
 *
 * ftbitmap_mmi_ext.h
 *
 *   Masked-quadword MMI bitmap kernels: GRAY8 / MONO embolden without
 *   scalar edge loops, and a four-way unrolled row OR (PS2 EE R5900).
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_MMI_EXT_H_
#define FTBITMAP_MMI_EXT_H_

#include <freetype/internal/ftmmi_mask_retro.h>

/* Every embolden kernel here visits whole 16-byte aligned quadwords that
 * overlap the row.  Quadwords are processed from the last to the first so
 * the previous quadword in memory is still original when it is read;
 * bytes outside the row are masked off on input and written back
 * unchanged.  QFSRV sets the shift-amount register, and three
 * instructions must separate it from the next MTSAB (R5900 hazard).
 */

#define FT_MMI_X_EMB_TAP( k )                  \
  "mtsab   $0, " #k "\n\t"                \
  "qfsrv   %4, %2, %3\n\t"                \
  "paddub  %5, %5, %4\n\t"                \
  "nop\n\t" "nop\n\t"

#define FT_MMI_X_EMB_QUAD( TAPS )                                              \
  __asm__ volatile (                                                      \
    ".set push\n\t" ".set noreorder\n\t"                                  \
    "lq      %1, 0(%0)\n\t"        /* original quad          */           \
    "lq      %3, 0(%6)\n\t"        /* previous quad          */           \
    "lq      %4, 0(%7)\n\t"        /* prev mask              */           \
    "lq      %2, 0(%8)\n\t"        /* in mask                */           \
    "pand    %3, %3, %4\n\t"                                              \
    "pand    %2, %1, %2\n\t"       /* cur = orig & inmask    */           \
    "por     %5, %2, $0\n\t"       /* acc = cur              */           \
    "nop\n\t" "nop\n\t"                                                   \
    TAPS                                                                  \
    "lq      %3, 0(%9)\n\t"                                               \
    "lq      %4, 0(%10)\n\t"                                              \
    "pxor    %2, %1, %5\n\t"       /* orig ^ acc             */           \
    "pand    %3, %3, %4\n\t"       /* out mask               */           \
    "pand    %2, %2, %3\n\t"                                              \
    "pxor    %1, %1, %2\n\t"       /* orig ^ ((orig^acc)&m)  */           \
    "sq      %1, 0(%0)\n\t"                                               \
    ".set pop\n\t"                                                        \
    : "+r" ( q ), "=&r" ( orig ), "=&r" ( cur ), "=&r" ( prev ),          \
      "=&r" ( t ), "=&r" ( acc )                                          \
    : "r" ( prevp ), "r" ( pm ), "r" ( im ), "r" ( om1 ), "r" ( om2 )     \
    : "memory" )

static void
ft_bitmap_mmi_gray8_embolden_mq( FT_Byte*  p,
                       FT_Int    n,
                       FT_Int    strength )
{
  FT_ULong  start = (FT_ULong)p;
  FT_ULong  last  = start + (FT_ULong)n - 1UL;
  FT_Byte*  q0    = (FT_Byte*)( start & ~15UL );
  FT_Int    nq    = (FT_Int)( ( ( last & ~15UL ) - (FT_ULong)q0 ) >> 4 ) + 1;
  const FT_Byte*  head = ft_mmi_head_mask[start & 15UL];
  const FT_Byte*  tail = ft_mmi_tail_mask[last & 15UL];
  FT_Int    i;


  if ( n <= 0 )
    return;

  for ( i = nq - 1; i >= 0; i-- )
  {
    FT_Byte*        q     = q0 + ( (FT_ULong)i << 4 );
    const FT_Byte*  prevp = i ? q - 16 : ft_mmi_zero16;
    const FT_Byte*  pm    = i == 1 ? head : ft_mmi_ones16;
    const FT_Byte*  im    = i == 0 ? head : ft_mmi_ones16;
    const FT_Byte*  om1   = i == 0 ? head : ft_mmi_ones16;
    const FT_Byte*  om2   = i == nq - 1 ? tail : ft_mmi_ones16;
    FT_ULong        orig, cur, prev, t, acc;


    switch ( strength )
    {
    case 1:  FT_MMI_X_EMB_QUAD( FT_MMI_X_EMB_TAP( 15 ) ); break;
    case 2:  FT_MMI_X_EMB_QUAD( FT_MMI_X_EMB_TAP( 15 ) FT_MMI_X_EMB_TAP( 14 ) ); break;
    case 3:  FT_MMI_X_EMB_QUAD( FT_MMI_X_EMB_TAP( 15 ) FT_MMI_X_EMB_TAP( 14 )
                           FT_MMI_X_EMB_TAP( 13 ) ); break;
    default: FT_MMI_X_EMB_QUAD( FT_MMI_X_EMB_TAP( 15 ) FT_MMI_X_EMB_TAP( 14 )
                           FT_MMI_X_EMB_TAP( 13 ) FT_MMI_X_EMB_TAP( 12 ) ); break;
    }
  }
}
#undef FT_MMI_X_EMB_QUAD
#undef FT_MMI_X_EMB_TAP

#define FT_MMI_X_R16( v )  v,v,v,v,v,v,v,v,v,v,v,v,v,v,v,v
static const FT_Byte ft_bitmap_mmi_mono_r[4][16] __attribute__(( aligned( 16 ) )) = {
  { FT_MMI_X_R16( 0x7F ) }, { FT_MMI_X_R16( 0x3F ) }, { FT_MMI_X_R16( 0x1F ) },
  { FT_MMI_X_R16( 0x0F ) }
};
static const FT_Byte ft_bitmap_mmi_mono_l[4][16] __attribute__(( aligned( 16 ) )) = {
  { FT_MMI_X_R16( 0x80 ) }, { FT_MMI_X_R16( 0xC0 ) }, { FT_MMI_X_R16( 0xE0 ) },
  { FT_MMI_X_R16( 0xF0 ) }
};
#undef FT_MMI_X_R16

#define FT_MMI_X_MONO_TAP( k, off )                \
  "psrlh   %5, %2, " #k "\n\t"                \
  "lq      %6, " #off "(%11)\n\t"             \
  "psllh   %7, %4, " #k "\n\t"                \
  "pand    %5, %5, %6\n\t"                    \
  "lq      %6, " #off "(%12)\n\t"             \
  "por     %8, %8, %5\n\t"                    \
  "pand    %7, %7, %6\n\t"                    \
  "por     %8, %8, %7\n\t"

/* shift counts for the left-moving previous byte are (8-k) */
#define FT_MMI_X_MONO_TAPS1  FT_MMI_X_MONO_TAP( 1, 0 )
#define FT_MMI_X_MONO_QUAD( TAPS )                                             \
  __asm__ volatile (                                                      \
    ".set push\n\t" ".set noreorder\n\t"                                  \
    "lq      %1, 0(%0)\n\t"                                               \
    "lq      %3, 0(%13)\n\t"                                              \
    "lq      %5, 0(%14)\n\t"                                              \
    "lq      %2, 0(%15)\n\t"                                              \
    "pand    %3, %3, %5\n\t"                                              \
    "pand    %2, %1, %2\n\t"                                              \
    "mtsab   $0, 15\n\t"                                                  \
    "por     %8, %2, $0\n\t"                                              \
    "nop\n\t" "nop\n\t"                                                   \
    "qfsrv   %4, %2, %3\n\t"                                              \
    "nop\n\t" "nop\n\t" "nop\n\t"                                         \
    TAPS                                                                  \
    "lq      %3, 0(%9)\n\t"                                               \
    "lq      %5, 0(%10)\n\t"                                              \
    "pxor    %2, %1, %8\n\t"                                              \
    "pand    %3, %3, %5\n\t"                                              \
    "pand    %2, %2, %3\n\t"                                              \
    "pxor    %1, %1, %2\n\t"                                              \
    "sq      %1, 0(%0)\n\t"                                               \
    ".set pop\n\t"                                                        \
    : "+r" ( q ), "=&r" ( orig ), "=&r" ( cur ), "=&r" ( prev ),          \
      "=&r" ( nb ), "=&r" ( t1 ), "=&r" ( mk ), "=&r" ( t2 ),            \
      "=&r" ( acc )                                                       \
    : "r" ( om1 ), "r" ( om2 ), "r" ( mr ), "r" ( ml ),                  \
      "r" ( prevp ), "r" ( pm ), "r" ( im )                               \
    : "memory" )

static void
ft_bitmap_mmi_mono_embolden_mq( FT_Byte*  p,
                      FT_Int    n,
                      FT_Int    strength )
{
  FT_ULong  start = (FT_ULong)p;
  FT_ULong  last  = start + (FT_ULong)n - 1UL;
  FT_Byte*  q0    = (FT_Byte*)( start & ~15UL );
  FT_Int    nq    = (FT_Int)( ( ( last & ~15UL ) - (FT_ULong)q0 ) >> 4 ) + 1;
  const FT_Byte*  head = ft_mmi_head_mask[start & 15UL];
  const FT_Byte*  tail = ft_mmi_tail_mask[last & 15UL];
  const FT_Byte*  mr = &ft_bitmap_mmi_mono_r[0][0];
  const FT_Byte*  ml = &ft_bitmap_mmi_mono_l[0][0];
  FT_Int    i;


  if ( n <= 0 )
    return;

  for ( i = nq - 1; i >= 0; i-- )
  {
    FT_Byte*        q     = q0 + ( (FT_ULong)i << 4 );
    const FT_Byte*  prevp = i ? q - 16 : ft_mmi_zero16;
    const FT_Byte*  pm    = i == 1 ? head : ft_mmi_ones16;
    const FT_Byte*  im    = i == 0 ? head : ft_mmi_ones16;
    const FT_Byte*  om1   = i == 0 ? head : ft_mmi_ones16;
    const FT_Byte*  om2   = i == nq - 1 ? tail : ft_mmi_ones16;
    FT_ULong        orig, cur, prev, nb, t1, mk, t2, acc;


    /* The "<< (8-k)" operand shifts must differ from the ">> k" ones, so
     * write each tap out with its own two immediates.                   */
    switch ( strength )
    {
    case 1:
      FT_MMI_X_MONO_QUAD(
        "psrlh   %5, %2, 1\n\t" "lq %6, 0(%11)\n\t" "psllh %7, %4, 7\n\t"
        "pand    %5, %5, %6\n\t" "lq %6, 0(%12)\n\t" "por %8, %8, %5\n\t"
        "pand    %7, %7, %6\n\t" "por %8, %8, %7\n\t" );
      break;
    case 2:
      FT_MMI_X_MONO_QUAD(
        "psrlh   %5, %2, 1\n\t" "lq %6, 0(%11)\n\t" "psllh %7, %4, 7\n\t"
        "pand    %5, %5, %6\n\t" "lq %6, 0(%12)\n\t" "por %8, %8, %5\n\t"
        "pand    %7, %7, %6\n\t" "por %8, %8, %7\n\t"
        "psrlh   %5, %2, 2\n\t" "lq %6, 16(%11)\n\t" "psllh %7, %4, 6\n\t"
        "pand    %5, %5, %6\n\t" "lq %6, 16(%12)\n\t" "por %8, %8, %5\n\t"
        "pand    %7, %7, %6\n\t" "por %8, %8, %7\n\t" );
      break;
    case 3:
      FT_MMI_X_MONO_QUAD(
        "psrlh   %5, %2, 1\n\t" "lq %6, 0(%11)\n\t" "psllh %7, %4, 7\n\t"
        "pand    %5, %5, %6\n\t" "lq %6, 0(%12)\n\t" "por %8, %8, %5\n\t"
        "pand    %7, %7, %6\n\t" "por %8, %8, %7\n\t"
        "psrlh   %5, %2, 2\n\t" "lq %6, 16(%11)\n\t" "psllh %7, %4, 6\n\t"
        "pand    %5, %5, %6\n\t" "lq %6, 16(%12)\n\t" "por %8, %8, %5\n\t"
        "pand    %7, %7, %6\n\t" "por %8, %8, %7\n\t"
        "psrlh   %5, %2, 3\n\t" "lq %6, 32(%11)\n\t" "psllh %7, %4, 5\n\t"
        "pand    %5, %5, %6\n\t" "lq %6, 32(%12)\n\t" "por %8, %8, %5\n\t"
        "pand    %7, %7, %6\n\t" "por %8, %8, %7\n\t" );
      break;
    default:
      FT_MMI_X_MONO_QUAD(
        "psrlh   %5, %2, 1\n\t" "lq %6, 0(%11)\n\t" "psllh %7, %4, 7\n\t"
        "pand    %5, %5, %6\n\t" "lq %6, 0(%12)\n\t" "por %8, %8, %5\n\t"
        "pand    %7, %7, %6\n\t" "por %8, %8, %7\n\t"
        "psrlh   %5, %2, 2\n\t" "lq %6, 16(%11)\n\t" "psllh %7, %4, 6\n\t"
        "pand    %5, %5, %6\n\t" "lq %6, 16(%12)\n\t" "por %8, %8, %5\n\t"
        "pand    %7, %7, %6\n\t" "por %8, %8, %7\n\t"
        "psrlh   %5, %2, 3\n\t" "lq %6, 32(%11)\n\t" "psllh %7, %4, 5\n\t"
        "pand    %5, %5, %6\n\t" "lq %6, 32(%12)\n\t" "por %8, %8, %5\n\t"
        "pand    %7, %7, %6\n\t" "por %8, %8, %7\n\t"
        "psrlh   %5, %2, 4\n\t" "lq %6, 48(%11)\n\t" "psllh %7, %4, 4\n\t"
        "pand    %5, %5, %6\n\t" "lq %6, 48(%12)\n\t" "por %8, %8, %5\n\t"
        "pand    %7, %7, %6\n\t" "por %8, %8, %7\n\t" );
      break;
    }
  }
}
#undef FT_MMI_X_MONO_QUAD
#undef FT_MMI_X_MONO_TAP
#undef FT_MMI_X_MONO_TAPS1

/* Row-length dispatch for GRAY8 embolden: masked quadwords for short rows
 * (no scalar edge loops), the software-pipelined block loop for long ones.
 */
static void
ft_bitmap_mmi_gray8_embolden_auto( FT_Byte*  p,
                         FT_Int    n,
                         FT_Int    strength )
{
  if ( n <= 96 )
    ft_bitmap_mmi_gray8_embolden_mq( p, n, strength );
  else
    ft_bitmap_mmi_gray8_embolden_small( p, n, strength );
}

static void
ft_bitmap_mmi_or_row_unroll( FT_Byte*        dst,
                   const FT_Byte*  src,
                   FT_Int          pitch )
{
  FT_Int  i = 0;


  if ( ( (FT_ULong)dst & 15UL ) == ( (FT_ULong)src & 15UL ) )
  {
    while ( i < pitch && ( (FT_ULong)( dst + i ) & 15UL ) != 0 )
    {
      dst[i] |= src[i];
      i++;
    }

    if ( pitch - i >= 64 )
    {
      FT_UInt   blocks = (FT_UInt)( ( pitch - i ) >> 6 );
      FT_ULong  d0, d1, d2, d3, s0, s1, s2, s3;
      FT_Byte*  dp = dst + i;
      const FT_Byte*  sp = src + i;


      i += (FT_Int)( blocks << 6 );
      __asm__ volatile (
        ".set push\n\t" ".set noreorder\n\t"
        "1:\n\t"
        "lq     %0, 0(%9)\n\t"
        "lq     %4, 0(%10)\n\t"
        "lq     %1, 16(%9)\n\t"
        "lq     %5, 16(%10)\n\t"
        "lq     %2, 32(%9)\n\t"
        "lq     %6, 32(%10)\n\t"
        "lq     %3, 48(%9)\n\t"
        "lq     %7, 48(%10)\n\t"
        "por    %0, %0, %4\n\t"
        "por    %1, %1, %5\n\t"
        "sq     %0, 0(%9)\n\t"
        "por    %2, %2, %6\n\t"
        "sq     %1, 16(%9)\n\t"
        "por    %3, %3, %7\n\t"
        "addiu  %8, %8, -1\n\t"
        "sq     %2, 32(%9)\n\t"
        "addiu  %9, %9, 64\n\t"
        "addiu  %10, %10, 64\n\t"
        "bgtz   %8, 1b\n\t"
        "sq     %3, -16(%9)\n\t"
        ".set pop\n\t"
        : "=&r" ( d0 ), "=&r" ( d1 ), "=&r" ( d2 ), "=&r" ( d3 ),
          "=&r" ( s0 ), "=&r" ( s1 ), "=&r" ( s2 ), "=&r" ( s3 ),
          "+r" ( blocks ), "+r" ( dp ), "+r" ( sp )
        :
        : "memory" );
    }

    for ( ; pitch - i >= 16; i += 16 )
    {
      FT_ULong  d, s;


      __asm__ volatile ( "lq  %0, 0(%2)\n\t"
                         "lq  %1, 0(%3)\n\t"
                         "por %0, %0, %1\n\t"
                         "sq  %0, 0(%2)\n\t"
                         : "=&r" ( d ), "=&r" ( s )
                         : "r" ( dst + i ), "r" ( src + i )
                         : "memory" );
    }
  }

  for ( ; i < pitch; i++ )
    dst[i] |= src[i];
}

#endif /* FTBITMAP_MMI_EXT_H_ */
