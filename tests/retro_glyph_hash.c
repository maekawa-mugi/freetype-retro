/* Deterministic glyph raster bitmap fingerprint for scalar/SIMD builds.
 *
 * Usage:
 *   retro-glyph-hash path/to/font.ttf [normal|mono|lcd|lcd-v]
 *
 * Use the SAME font file with the scalar and SIMD FreeType libraries.
 * The program hashes glyph metrics and every allocated bitmap byte,
 * including pitch, so it can detect padding/row-order differences.
 */
#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t hash64 = UINT64_C( 14695981039346656037 );

static void
hash_byte( unsigned v )
{
  hash64 ^= (unsigned char)v;
  hash64 *= UINT64_C( 1099511628211 );
}

static void
hash_u32( uint32_t v )
{
  unsigned i;

  for ( i = 0; i < 4; i++, v >>= 8 )
    hash_byte( v );
}

static void
hash_bitmap( FT_GlyphSlot slot )
{
  const FT_Bitmap* b = &slot->bitmap;
  size_t           size;
  FT_UInt          pitch;
  size_t           i;


  hash_u32( (uint32_t)slot->bitmap_left );
  hash_u32( (uint32_t)slot->bitmap_top );
  hash_u32( (uint32_t)slot->advance.x );
  hash_u32( (uint32_t)slot->advance.y );
  hash_u32( (uint32_t)b->width );
  hash_u32( (uint32_t)b->rows );
  hash_u32( (uint32_t)b->pitch );
  hash_u32( (uint32_t)b->pixel_mode );

  pitch = (FT_UInt)( b->pitch < 0 ? -b->pitch : b->pitch );
  size  = (size_t)pitch * b->rows;
  for ( i = 0; i < size; ++i )
    hash_byte( b->buffer[i] );
}

int
main( int argc, char** argv )
{
  FT_Library     lib;
  FT_Face        face;
  FT_Render_Mode mode = FT_RENDER_MODE_NORMAL;
  unsigned      si;
  FT_ULong       cp;
  unsigned long  rendered = 0;
  const FT_UInt  sizes[] = { 8, 12, 18, 32, 64, 128 };


  if ( argc < 2 || argc > 3 )
  {
    fprintf( stderr, "usage: %s font.ttf [normal|mono|lcd|lcd-v]\n",
             argv[0] );
    return 2;
  }

  if ( argc == 3 )
  {
    if      ( !strcmp( argv[2], "mono" ) )
      mode = FT_RENDER_MODE_MONO;
    else if ( !strcmp( argv[2], "lcd" ) )
      mode = FT_RENDER_MODE_LCD;
    else if ( !strcmp( argv[2], "lcd-v" ) )
      mode = FT_RENDER_MODE_LCD_V;
    else if ( strcmp( argv[2], "normal" ) )
    {
      fprintf( stderr, "unknown render mode: %s\n", argv[2] );
      return 2;
    }
  }

  if ( FT_Init_FreeType( &lib ) )
  {
    fprintf( stderr, "FT_Init_FreeType failed\n" );
    return 1;
  }

  if ( FT_New_Face( lib, argv[1], 0, &face ) )
  {
    fprintf( stderr, "FT_New_Face failed\n" );
    FT_Done_FreeType( lib );
    return 1;
  }

  for ( si = 0; si < sizeof( sizes ) / sizeof( sizes[0] ); ++si )
  {
    if ( FT_Set_Pixel_Sizes( face, 0, sizes[si] ) )
    {
      fprintf( stderr, "FT_Set_Pixel_Sizes failed for %u\n", sizes[si] );
      FT_Done_Face( face );
      FT_Done_FreeType( lib );
      return 1;
    }

    /* Dense ASCII, hiragana, katakana and an initial kanji range.
     * Unsupported glyphs are skipped identically across both builds.
     */
    for ( cp = 0x21; cp <= 0x4E7F; ++cp )
    {
      FT_UInt index;

      if ( !( cp <= 0x7E ||
              ( cp >= 0x3040 && cp <= 0x30FF ) ||
              ( cp >= 0x4E00 && cp <= 0x4E7F ) ) )
        continue;

      index = FT_Get_Char_Index( face, cp );
      if ( !index )
        continue;

      if ( FT_Load_Glyph( face, index, FT_LOAD_DEFAULT ) ||
           FT_Render_Glyph( face->glyph, mode ) )
      {
        fprintf( stderr, "Failed glyph U+%04lX at %u px\n", cp, sizes[si] );
        FT_Done_Face( face );
        FT_Done_FreeType( lib );
        return 1;
      }

      hash_u32( (uint32_t)cp );
      hash_u32( (uint32_t)sizes[si] );
      hash_bitmap( face->glyph );
      rendered++;
    }
  }

  FT_Done_Face( face );
  FT_Done_FreeType( lib );
  printf( "glyphs=%lu FNV-1a-64=%016llx\n",
          rendered, (unsigned long long)hash64 );
  return 0;
}
