/****************************************************************************
 *
 * ftbitmap_bgra_gray_retro.h
 *
 *   Exact optional BGRA -> grayscale component-square lookups.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTBITMAP_BGRA_GRAY_RETRO_H_
#define FTBITMAP_BGRA_GRAY_RETRO_H_

/* The scalar source computes
 *
 *   l = (4731UL*B*B + 46868UL*G*G + 13937UL*R*R) >> 16;
 *   output = A ? (FT_Byte)(A - l/A) : 0;
 *
 * PRECOMPUTING EACH FULL UNSHIFTED CONTRIBUTION is essential.
 * Shifting each table entry before summation would change rounding.
 * For all B,G,R in 0..255, their sum is at most 65536*65025,
 * which fits in unsigned 32-bit arithmetic.  The 3x256 table uses
 * 3072 bytes of stack storage, with no dynamic allocation.
 */
typedef struct  FT_Retro_BGRA_Gray_Table_
{
  FT_UInt32  square[3][256];

} FT_Retro_BGRA_Gray_Table;


static void
ft_bitmap_retro_bgra_gray_prepare( FT_Retro_BGRA_Gray_Table*  table )
{
  FT_UInt  i;


  for ( i = 0; i < 256; i++ )
  {
    FT_UInt32  square = (FT_UInt32)i * i;


    table->square[0][i] = 4731UL  * square;
    table->square[1][i] = 46868UL * square;
    table->square[2][i] = 13937UL * square;
  }
}


static void
ft_bitmap_retro_bgra_gray_row_div( FT_Byte*                         dst,
                                const FT_Byte*                   src,
                                FT_UInt                          width,
                                const FT_Retro_BGRA_Gray_Table*   table )
{
  FT_UInt  i;


  for ( i = 0; i < width; ++i )
  {
    FT_UInt  a = src[3];


    if ( !a )
      dst[i] = 0;
    else
    {
      FT_UInt32  l = ( table->square[0][src[0]] +
                       table->square[1][src[1]] +
                       table->square[2][src[2]] ) >> 16;


      dst[i] = (FT_Byte)( a - l / a );
    }

    src += 4;
  }
}

/* ceil(65536/a) gives an estimate at most one above floor(n/a),
 * for 0 <= n <= 65025.  One unsigned correction makes it exact.
 * The product is at most 65025*65536 and fits in 32 bits.
 */
static const FT_UInt32 ft_bgra_gray_reciprocal[256] = {
  0U, 65536U, 32768U, 21846U, 16384U, 13108U, 10923U, 9363U,
  8192U, 7282U, 6554U, 5958U, 5462U, 5042U, 4682U, 4370U,
  4096U, 3856U, 3641U, 3450U, 3277U, 3121U, 2979U, 2850U,
  2731U, 2622U, 2521U, 2428U, 2341U, 2260U, 2185U, 2115U,
  2048U, 1986U, 1928U, 1873U, 1821U, 1772U, 1725U, 1681U,
  1639U, 1599U, 1561U, 1525U, 1490U, 1457U, 1425U, 1395U,
  1366U, 1338U, 1311U, 1286U, 1261U, 1237U, 1214U, 1192U,
  1171U, 1150U, 1130U, 1111U, 1093U, 1075U, 1058U, 1041U,
  1024U, 1009U, 993U, 979U, 964U, 950U, 937U, 924U,
  911U, 898U, 886U, 874U, 863U, 852U, 841U, 830U,
  820U, 810U, 800U, 790U, 781U, 772U, 763U, 754U,
  745U, 737U, 729U, 721U, 713U, 705U, 698U, 690U,
  683U, 676U, 669U, 662U, 656U, 649U, 643U, 637U,
  631U, 625U, 619U, 613U, 607U, 602U, 596U, 591U,
  586U, 580U, 575U, 570U, 565U, 561U, 556U, 551U,
  547U, 542U, 538U, 533U, 529U, 525U, 521U, 517U,
  512U, 509U, 505U, 501U, 497U, 493U, 490U, 486U,
  482U, 479U, 475U, 472U, 469U, 465U, 462U, 459U,
  456U, 452U, 449U, 446U, 443U, 440U, 437U, 435U,
  432U, 429U, 426U, 423U, 421U, 418U, 415U, 413U,
  410U, 408U, 405U, 403U, 400U, 398U, 395U, 393U,
  391U, 388U, 386U, 384U, 382U, 379U, 377U, 375U,
  373U, 371U, 369U, 367U, 365U, 363U, 361U, 359U,
  357U, 355U, 353U, 351U, 349U, 347U, 345U, 344U,
  342U, 340U, 338U, 337U, 335U, 333U, 331U, 330U,
  328U, 327U, 325U, 323U, 322U, 320U, 319U, 317U,
  316U, 314U, 313U, 311U, 310U, 308U, 307U, 305U,
  304U, 303U, 301U, 300U, 298U, 297U, 296U, 294U,
  293U, 292U, 290U, 289U, 288U, 287U, 285U, 284U,
  283U, 282U, 281U, 279U, 278U, 277U, 276U, 275U,
  274U, 272U, 271U, 270U, 269U, 268U, 267U, 266U,
  265U, 264U, 263U, 262U, 261U, 260U, 259U, 258U,
};

static FT_UInt
ft_bitmap_retro_gray_divide( FT_UInt n, FT_UInt a )
{
  FT_UInt q;
  if ( a == 255U )
    return ( n + 1U + ( n >> 8 ) ) >> 8;
  q = ( n * ft_bgra_gray_reciprocal[a] ) >> 16;
  return q - ( q * a > n );
}

static void
ft_bitmap_retro_bgra_gray_row( FT_Byte* dst, const FT_Byte* src,
                              FT_UInt width,
                              const FT_Retro_BGRA_Gray_Table* table )
{
  FT_UInt i;
  for ( i = 0; i < width; i++, src += 4 )
  {
    FT_UInt a = src[3];
    if ( !a )
      dst[i] = 0;
    else
    {
      FT_UInt32 l = ( table->square[0][src[0]] +
                      table->square[1][src[1]] +
                      table->square[2][src[2]] ) >> 16;
      dst[i] = (FT_Byte)( a - ft_bitmap_retro_gray_divide( l, a ) );
    }
  }
}

#endif /* FTBITMAP_BGRA_GRAY_RETRO_H_ */
