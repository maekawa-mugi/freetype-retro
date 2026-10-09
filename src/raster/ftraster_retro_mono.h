/****************************************************************************
 *
 * ftraster_retro_mono.h
 *
 *   Optional long, fully opaque span fill for the monochrome raster.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.
 *
 */

#ifndef FTRASTER_RETRO_MONO_H_
#define FTRASTER_RETRO_MONO_H_

/* The raster's MONO span routine already handles partial edge bytes
 * with OR masks.  Only the full interior bytes may be overwritten.
 * Small spans keep the original byte-at-a-time loop.
 */
#define FT_RASTER_RETRO_MONO_MIN_BYTES  64

static void
ft_raster_retro_mono_fill( unsigned char*  dst,
                           int             count )
{
#if defined( FT_CONFIG_OPTION_MMI_MONO_SPANS ) && \
    !defined( FT_CONFIG_OPTION_NO_ASSEMBLER )

#if !defined( __mips__ ) || !defined( __GNUC__ )
#error "FT_CONFIG_OPTION_MMI_MONO_SPANS requires GCC-compatible R5900 MMI"
#endif

  unsigned char  buffer[31];
  unsigned char* pattern;
  unsigned char* p = dst;
  unsigned int   n = (unsigned int)count;
  unsigned int   blocks, i;


  pattern = (unsigned char*)( ( (FT_ULong)buffer + 15UL ) &
                              ~(FT_ULong)15UL );
  for ( i = 0; i < 16; i++ )
    pattern[i] = 0xFF;

  while ( n && ( (FT_ULong)p & 15UL ) )
  {
    *p++ = 0xFF;
    n--;
  }

  blocks = n >> 4;
  if ( blocks )
  {
    FT_ULong  word;


    /* LQ/SQ addresses are always 16-byte aligned.  Only the full
     * interior of a MONO span is written, with no out-of-row stores.
     */
    __asm__ volatile (
      ".set push\n\t"
      ".set noreorder\n\t"
      "lq    %0, 0(%3)\n\t"
      "1:\n\t"
      "sq    %0, 0(%1)\n\t"
      "addiu %1, %1, 16\n\t"
      "addiu %2, %2, -1\n\t"
      "bne   %2, $zero, 1b\n\t"
      "nop\n\t"
      ".set pop\n\t"
      : "=&r" ( word ), "+r" ( p ), "+r" ( blocks )
      : "r" ( pattern )
      : "memory" );
    n &= 15U;
  }

  while ( n-- )
    *p++ = 0xFF;

#elif defined( FT_CONFIG_OPTION_VIS1_MONO_SPANS ) && \
      !defined( FT_CONFIG_OPTION_NO_ASSEMBLER )

#if !defined( __sparc__ ) || !defined( __GNUC__ )
#error "FT_CONFIG_OPTION_VIS1_MONO_SPANS requires GCC-compatible SPARC"
#endif

  unsigned char  buffer[15];
  unsigned char* pattern;
  unsigned char* p = dst;
  unsigned int   n = (unsigned int)count;
  unsigned int   blocks, i;


  pattern = (unsigned char*)( ( (FT_ULong)buffer + 7UL ) &
                              ~(FT_ULong)7UL );
  for ( i = 0; i < 8; i++ )
    pattern[i] = 0xFF;

  while ( n && ( (FT_ULong)p & 7UL ) )
  {
    *p++ = 0xFF;
    n--;
  }

  blocks = n >> 3;
  if ( blocks )
  {
    __asm__ volatile (
      "ldd [%2], %%f0\n\t"
      "1:\n\t"
      "std %%f0, [%0]\n\t"
      "add %0, 8, %0\n\t"
      "subcc %1, 1, %1\n\t"
      "bne 1b\n\t"
      "nop\n\t"
      : "+r" ( p ), "+r" ( blocks )
      : "r" ( pattern )
      : "memory", "cc", "f0", "f1" );
    n &= 7U;
  }

  while ( n-- )
    *p++ = 0xFF;

#else

  FT_MEM_SET( dst, 0xFF, count );

#endif
}

#endif /* FTRASTER_RETRO_MONO_H_ */
