/****************************************************************************
 *
 * ftmmi_mask_retro.h
 *
 *   Shared masked-quadword helpers for the PS2 R5900 MMI kernels.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTMMI_MASK_RETRO_H_
#define FTMMI_MASK_RETRO_H_

#if !defined( _MIPS_ARCH_R5900 ) || !defined( __GNUC__ )
#error "ftmmi_mask_retro.h requires GCC for the R5900"
#endif

/* The kernels that use these tables read and write whole 16-byte aligned
 * quadwords that merely overlap the destination range.  Bytes outside the
 * range are written back with their original values, so memory outside the
 * range is never changed -- but it is touched, and the quadword must lie
 * inside mapped memory (always true for an allocation that straddles it).
 */

/* head_mask[o]: lanes o..15 = 0xFF.  tail_mask[e]: lanes 0..e = 0xFF. */
static const FT_Byte ft_mmi_head_mask[16][16] __attribute__(( aligned( 16 ) )) = {
  {255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255},
  {0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255},
  {0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255},
  {0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255},
  {0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255},
  {0,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255},
  {0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,255},
  {0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255},
  {0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255},
  {0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255},
  {0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255},
  {0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255},
  {0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255}
};
static const FT_Byte ft_mmi_tail_mask[16][16] __attribute__(( aligned( 16 ) )) = {
  {255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0},
  {255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0},
  {255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0},
  {255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0},
  {255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0},
  {255,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0},
  {255,255,255,255,255,255,255,255,255,255,0,0,0,0,0,0},
  {255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,0},
  {255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0},
  {255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0},
  {255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0},
  {255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0},
  {255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255}
};

static const FT_Byte  ft_mmi_ones16[16]
  __attribute__(( aligned( 16 ) )) = {
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};
static const FT_Byte  ft_mmi_zero16[16]
  __attribute__(( aligned( 16 ) )) = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* q (16-byte aligned) += pat & m1 & m2, bytewise modulo 256. */
static inline void
ft_mmi_rmw_masked( FT_Byte*        q,
                   const FT_Byte*  pat,
                   const FT_Byte*  m1,
                   const FT_Byte*  m2 )
{
  FT_ULong  v, p, m;


  __asm__ volatile ( "lq     %0, 0(%3)\n\t"
                     "lq     %1, 0(%4)\n\t"
                     "lq     %2, 0(%5)\n\t"
                     "pand   %1, %1, %2\n\t"
                     "lq     %2, 0(%6)\n\t"
                     "pand   %1, %1, %2\n\t"
                     "paddb  %0, %0, %1\n\t"
                     "sq     %0, 0(%3)\n\t"
                     : "=&r" ( v ), "=&r" ( p ), "=&r" ( m )
                     : "r" ( q ), "r" ( pat ), "r" ( m1 ), "r" ( m2 )
                     : "memory" );
}

#endif /* FTMMI_MASK_RETRO_H_ */
