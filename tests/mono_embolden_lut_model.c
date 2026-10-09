/* Original MONO embolden row vs real 512-byte table kernel.
 * The right-to-left original byte update must be preserved exactly.
 * No MMI/SPARC instructions are executed.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t FT_Byte;
typedef unsigned int FT_UInt;
typedef int FT_Int;
#include "../src/base/ftbitmap_mono_embolden_retro.h"

static uint32_t seed=UINT32_C(0x97a35e41);
static unsigned long cases,bytes;
static uint32_t
random32(void)
{
  uint32_t x=seed;
  x^=x<<13;
  x^=x>>17;
  x^=x<<5;
  return seed=x;
}
static void
scalar(FT_Byte* p,int pitch,unsigned strength)
{
  int x;
  unsigned i;

  for(x=pitch-1;x>=0;x--)
  {
    unsigned char tmp=p[x];
    for(i=1;i<=strength;i++)
    {
      p[x]|=(FT_Byte)(tmp>>i);
      if(x>0)
        p[x]|=(FT_Byte)(p[x-1]<<(8-i));
    }
  }
}
static void
check(const FT_Retro_Mono_Embolden_Table* table,
      unsigned strength,unsigned pitch,unsigned offset)
{
  FT_Byte src[1168],dst[1168],ref[1168];
  unsigned j;

  for(j=0;j<sizeof(src);j++)
    src[j]=(FT_Byte)random32();
  memcpy(dst,src,sizeof(dst));
  memcpy(ref,src,sizeof(ref));

  scalar(ref+32+offset,(int)pitch,strength);
  ft_bitmap_retro_mono_embolden_row(dst+32+offset,(int)pitch,table);
  if(memcmp(dst,ref,sizeof(ref)))
  {
    fprintf(stderr,"FAIL MONO embolden LUT str=%u pitch=%u offset=%u\n",
            strength,pitch,offset);
    exit(1);
  }
  cases++;
  bytes+=pitch;
}
int
main(void)
{
  FT_Retro_Mono_Embolden_Table table;
  unsigned str,pitch,offset,k;

  for(str=1;str<=8;str++)
  {
    ft_bitmap_retro_mono_embolden_prepare(&table,str);
    for(pitch=0;pitch<=257;pitch++)
      for(offset=0;offset<16;offset++)
        check(&table,str,pitch,offset);

    for(k=0;k<6000;k++)
      check(&table,str,random32()%1025U,k%16U);
  }

  printf("PASS MONO horizontal embolden LUT: %lu rows, %lu bytes\n",
         cases,bytes);
  return 0;
}
