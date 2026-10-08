/* Deterministic FT_Bitmap_Convert regression fingerprint.
 *
 * Compile against scalar and optional SIMD variants of the same FreeType
 * revision.  Identical fingerprints show bit-for-bit equivalence for
 * the ACTIVE pixels; unused alignment padding is intentionally excluded
 * because FT_Bitmap_Convert does not promise to initialize those bytes.
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t  state = UINT32_C( 0x8C3C995F );
static uint64_t  hash  = UINT64_C( 14695981039346656037 );
static unsigned long cases = 0;

static uint32_t
rand32( void )
{
  uint32_t x = state;


  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}

static void
hash_byte( unsigned char value )
{
  hash ^= value;
  hash *= UINT64_C( 1099511628211 );
}

static void
hash_word( uint32_t value )
{
  int n;


  for ( n = 0; n < 4; ++n, value >>= 8 )
    hash_byte( (unsigned char)value );
}

static int
one_case( FT_Library library,
          FT_Byte mode, FT_UInt width, FT_UInt height,
          FT_UInt src_padding, FT_UInt alignment,
          int negative_src, int negative_dst )
{
  FT_Bitmap  src, dst;
  FT_Byte*   buffer;
  FT_UInt    src_bytes, stride, row;
  FT_Error   error;
  size_t     bytes;


  FT_Bitmap_Init( &src );
  FT_Bitmap_Init( &dst );

  src_bytes = mode == FT_PIXEL_MODE_MONO ? ( width + 7 ) >> 3
            : mode == FT_PIXEL_MODE_GRAY2 ? ( width + 3 ) >> 2
                                           : ( width + 1 ) >> 1;
  stride = src_bytes + src_padding;
  bytes = (size_t)stride * height;
  buffer = (FT_Byte*)malloc( bytes );
  if ( !buffer )
  {
    fprintf( stderr, "malloc failed\n" );
    return 0;
  }

  for ( row = 0; row < bytes; ++row )
    buffer[row] = (FT_Byte)rand32();

  src.width      = width;
  src.rows       = height;
  src.pitch      = negative_src ? -(FT_Int)stride : (FT_Int)stride;
  src.pixel_mode = mode;
  src.buffer     = buffer;
  src.num_grays  = (FT_UShort)( mode == FT_PIXEL_MODE_MONO ? 2 :
                                mode == FT_PIXEL_MODE_GRAY2 ? 4 : 16 );
  if ( negative_dst )
    dst.pitch = -1;

  error = FT_Bitmap_Convert( library, &src, &dst, (FT_Int)alignment );
  free( buffer );
  if ( error )
  {
    fprintf( stderr, "Bitmap_Convert error %d for mode %u, width %u\n",
             error, (unsigned)mode, width );
    return 0;
  }

  if ( dst.width != width || dst.rows != height ||
       dst.pixel_mode != FT_PIXEL_MODE_GRAY ||
       dst.num_grays != src.num_grays )
  {
    fprintf( stderr, "Unexpected converted bitmap metadata\n" );
    FT_Bitmap_Done( library, &dst );
    return 0;
  }

  hash_word( mode );
  hash_word( width );
  hash_word( height );
  hash_word( src_padding );
  hash_word( alignment );
  hash_word( (uint32_t)negative_src );
  hash_word( (uint32_t)negative_dst );
  hash_word( (uint32_t)dst.pitch );

  /* Compare physical row order including negative-pitch cases, but
   * intentionally skip any untouched bytes after 'width' in a row. */
  stride = (FT_UInt)( dst.pitch < 0 ? -dst.pitch : dst.pitch );
  for ( row = 0; row < height; ++row )
  {
    FT_UInt j;
    const FT_Byte* row_start = dst.buffer + (size_t)row * stride;


    for ( j = 0; j < width; ++j )
      hash_byte( row_start[j] );
  }

  error = FT_Bitmap_Done( library, &dst );
  if ( error )
  {
    fprintf( stderr, "FT_Bitmap_Done failed\n" );
    return 0;
  }
  ++cases;
  return 1;
}

int
main( void )
{
  FT_Library lib;
  const FT_Byte modes[] = { FT_PIXEL_MODE_MONO,
                            FT_PIXEL_MODE_GRAY2,
                            FT_PIXEL_MODE_GRAY4 };
  const FT_UInt widths[] = { 1, 2, 3, 4, 7, 8, 9, 15, 16, 17,
                             23, 31, 32, 33, 63, 64, 65, 127,
                             128, 129, 255, 256, 257 };
  const FT_UInt heights[] = { 1, 2, 5 };
  const FT_UInt pads[] = { 0, 1, 7, 15 };
  const FT_UInt alignments[] = { 1, 2, 4, 8, 16 };
  unsigned m,w,h,p,a,ns,nd;


  if ( FT_Init_FreeType( &lib ) )
  {
    fprintf( stderr, "FT_Init_FreeType failed\n" );
    return 1;
  }

  for ( m=0; m<sizeof(modes)/sizeof(modes[0]); ++m )
    for ( w=0; w<sizeof(widths)/sizeof(widths[0]); ++w )
      for ( h=0; h<sizeof(heights)/sizeof(heights[0]); ++h )
        for ( p=0; p<sizeof(pads)/sizeof(pads[0]); ++p )
          for ( a=0; a<sizeof(alignments)/sizeof(alignments[0]); ++a )
            for ( ns=0; ns<2; ++ns )
              for ( nd=0; nd<2; ++nd )
                if ( !one_case( lib, modes[m], widths[w], heights[h],
                                pads[p], alignments[a],
                                (int)ns, (int)nd ) )
                {
                  FT_Done_FreeType( lib );
                  return 1;
                }

  FT_Done_FreeType( lib );
  printf( "FT_Bitmap_Convert: %lu cases; FNV-1a-64 %016llx\n",
          cases, (unsigned long long)hash );
  return 0;
}
