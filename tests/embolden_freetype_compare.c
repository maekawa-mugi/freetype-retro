/* Deterministic black-box FT_Bitmap_Embolden output fingerprint.
 *
 * Build the whole library twice (default scalar, optional architecture SIMD).
 * Build this program against each variant.  Matching fingerprints show
 * matching bitmap bytes for this deterministic test matrix.
 *
 * This test is not a microbenchmark.  Never compare runtimes of this tool to
 * infer the performance of a kernel.
 */

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t seed = UINT32_C( 0x59A68CF1 );
static uint64_t digest = UINT64_C( 14695981039346656037 );
static unsigned long cases = 0;

static uint32_t
next_random( void )
{
  uint32_t x = seed;

  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  seed = x;
  return x;
}

static void
digest_byte( unsigned char x )
{
  digest ^= x;
  digest *= UINT64_C( 1099511628211 );
}

static void
digest_word( uint32_t x )
{
  unsigned i;

  for ( i = 0; i < 4; ++i )
  {
    digest_byte( (unsigned char)x );
    x >>= 8;
  }
}

static int
run_case( FT_Library library, FT_UInt width, FT_UInt pad,
          FT_Int xstr, FT_Int ystr, FT_Byte mode, int negative )
{
  FT_Bitmap  source;
  FT_Bitmap  result;
  FT_UInt    pitch, rows = 5, i;
  FT_Error   error;
  size_t     size;

  FT_Bitmap_Init( &source );
  FT_Bitmap_Init( &result );

  switch ( mode )
  {
  case FT_PIXEL_MODE_MONO:  pitch = ( width + 7 ) / 8; break;
  case FT_PIXEL_MODE_GRAY2: pitch = ( width + 3 ) / 4; break;
  case FT_PIXEL_MODE_GRAY4: pitch = ( width + 1 ) / 2; break;
  case FT_PIXEL_MODE_BGRA:  pitch = width * 4; break;
  default:                  pitch = width; break;
  }

  pitch += pad;
  size = (size_t)pitch * rows;
  source.buffer = (FT_Byte*)malloc( size );
  if ( !source.buffer )
  {
    fprintf( stderr, "out of memory\n" );
    return 0;
  }

  for ( i = 0; i < size; ++i )
    source.buffer[i] = (FT_Byte)next_random();

  source.width      = width;
  source.rows       = rows;
  source.pitch      = negative ? -(FT_Int)pitch : (FT_Int)pitch;
  source.pixel_mode = mode;
  source.num_grays  = (FT_UShort)( mode == FT_PIXEL_MODE_MONO  ? 2   :
                                   mode == FT_PIXEL_MODE_GRAY2 ? 4   :
                                   mode == FT_PIXEL_MODE_GRAY4 ? 16  :
                                                                  256 );

  error = FT_Bitmap_Copy( library, &source, &result );
  free( source.buffer );
  if ( error )
  {
    fprintf( stderr, "FT_Bitmap_Copy error %d\n", error );
    return 0;
  }

  error = FT_Bitmap_Embolden( library, &result,
                             (FT_Pos)xstr * 64, (FT_Pos)ystr * 64 );
  if ( error )
  {
    fprintf( stderr,
             "FT_Bitmap_Embolden error %d; mode=%u width=%u pad=%u "
             "xstr=%d ystr=%d negative=%d\n",
             error, (unsigned)mode, width, pad, xstr, ystr, negative );
    FT_Bitmap_Done( library, &result );
    return 0;
  }

  digest_word( (uint32_t)result.width );
  digest_word( (uint32_t)result.rows );
  digest_word( (uint32_t)result.pitch );
  digest_word( (uint32_t)result.pixel_mode );
  digest_word( (uint32_t)result.num_grays );

  pitch = (FT_UInt)( result.pitch < 0 ? -result.pitch : result.pitch );
  size  = (size_t)pitch * result.rows;
  if ( size && !result.buffer )
  {
    fprintf( stderr, "unexpected empty bitmap output\n" );
    FT_Bitmap_Done( library, &result );
    return 0;
  }

  for ( i = 0; i < size; ++i )
    digest_byte( result.buffer[i] );

  FT_Bitmap_Done( library, &result );
  ++cases;
  return 1;
}

int
main( void )
{
  FT_Library  library;
  FT_Error    error;
  unsigned   a, b, x, y, m, negative;

  const FT_UInt widths[] = { 1, 2, 3, 7, 8, 15, 16, 17,
                             31, 32, 33, 63, 64, 65, 127 };
  const FT_UInt pads[] = { 0, 1, 7, 15 };
  const FT_Int ys[] = { 0, 1, 3 };
  const FT_Byte modes[] = { FT_PIXEL_MODE_MONO,
                            FT_PIXEL_MODE_GRAY,
                            FT_PIXEL_MODE_GRAY2,
                            FT_PIXEL_MODE_GRAY4,
                            FT_PIXEL_MODE_LCD,
                            FT_PIXEL_MODE_LCD_V,
                            FT_PIXEL_MODE_BGRA };

  error = FT_Init_FreeType( &library );
  if ( error )
  {
    fprintf( stderr, "FT_Init_FreeType error %d\n", error );
    return 1;
  }

  for ( a = 0; a < sizeof( widths ) / sizeof( widths[0] ); ++a )
    for ( b = 0; b < sizeof( pads ) / sizeof( pads[0] ); ++b )
      for ( x = 0; x <= 4; ++x )
        for ( y = 0; y < sizeof( ys ) / sizeof( ys[0] ); ++y )
          for ( m = 0; m < sizeof( modes ) / sizeof( modes[0] ); ++m )
            for ( negative = 0; negative <= 1; ++negative )
              if ( !run_case( library, widths[a], pads[b], (FT_Int)x,
                              ys[y], modes[m], (int)negative ) )
              {
                FT_Done_FreeType( library );
                return 1;
              }

  FT_Done_FreeType( library );

  printf( "FT_Bitmap_Embolden: %lu cases; FNV-1a-64 %016llx\n",
          cases, (unsigned long long)digest );
  return 0;
}
