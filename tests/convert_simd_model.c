/* Portable byte-exact model of opt-in packed-to-gray conversion.
 * This checks the intended MMI/ VIS1 byte mapping, not target machine code.
 * Actual R5900 / SPARC execution is a separate deferred test.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { MONO = 1, GRAY2 = 2, GRAY4 = 4 };
static unsigned long cases, mmi_blocks, vis_blocks;

static unsigned
pixel(const uint8_t* src, unsigned x, unsigned bpp)
{
  unsigned perbyte = 8 / bpp;
  return (src[x / perbyte] >> (8 - bpp * (x % perbyte + 1))) &
         ((1U << bpp) - 1);
}

static void
reference(const uint8_t* src, uint8_t* dst, unsigned width, unsigned bpp)
{
  unsigned x;
  for (x=0; x<width; x++) dst[x] = (uint8_t)pixel(src,x,bpp);
}

static void
mmi_model(const uint8_t* src, uint8_t* dst, unsigned width, unsigned bpp)
{
  unsigned x=0, block, k;
  if (((uintptr_t)dst & 15) == 0 &&
      (bpp!=GRAY4 || (((uintptr_t)src & 7) == 0))) {
    while(width-x>=16) {
      uint8_t temp[16];
      for(k=0;k<16;k++) {
        if(bpp==GRAY4) {
          /* Simulate PEXTLB -> PSRLH(4), PSLLH(12), PSRLH(4), POR.
           * PEXTLB unpacks input byte to a 16-bit little-endian lane.
           */
          uint16_t lane=src[(x+k)/2];
          uint16_t hi=lane>>4;
          uint16_t lo=(uint16_t)((lane << 12) & 65535U)>>4;
          uint16_t packed=hi|lo;
          temp[k]=(uint8_t)(k&1?packed>>8:packed);
        } else temp[k]=(uint8_t)pixel(src,x+k,bpp);
      }
      for(block=0;block<16;block++) dst[x+block]=temp[block];
      x+=16;
      ++mmi_blocks;
    }
  }
  for (;x<width;++x) dst[x]=(uint8_t)pixel(src,x,bpp);
}

static void
vis1_model(const uint8_t* src, uint8_t* dst, unsigned width, unsigned bpp)
{
  unsigned x=0;
  if (((uintptr_t)dst & 7) == 0) {
    while(width-x>=8) {
      uint32_t high=0,low=0;
      unsigned j;
      for(j=0;j<4;j++) {
        unsigned a=pixel(src,x+j*2,bpp);
        unsigned b=pixel(src,x+j*2+1,bpp);
        high=(high<<8)|a;
        low=(low<<8)|b;
      }
      /* VIS1 FPMERGE reads high/low from BE 32-bit words and
       * interleaves corresponding bytes into the 64-bit result. */
      for(j=0;j<4;j++) {
        dst[x+j*2]=(uint8_t)(high >> (24-8*j));
        dst[x+j*2+1]=(uint8_t)(low >> (24-8*j));
      }
      x+=8;
      ++vis_blocks;
    }
  }
  for (;x<width;++x) dst[x]=(uint8_t)pixel(src,x,bpp);
}

int main(void)
{
  uint8_t source[512], gold[512], mmi[512], vis[512];
  unsigned bpp, width, sa,da,i;
  for(bpp=1;bpp<=4;bpp*=2)
    for(width=1;width<=257;++width)
      for(sa=0;sa<16;++sa)
        for(da=0;da<16;++da) {
          uint8_t *s=source+64+sa;
          uint8_t *g=gold+64+da;
          uint8_t *m=mmi+64+da;
          uint8_t *v=vis+64+da;
          for(i=0;i<512;i++) source[i]=(uint8_t)((i*157U+width*13U+sa*3U)&255);
          memset(gold,0xA5,sizeof(gold));
          memset(mmi,0xA5,sizeof(mmi));
          memset(vis,0xA5,sizeof(vis));
          reference(s,g,width,bpp);
          mmi_model(s,m,width,bpp);
          vis1_model(s,v,width,bpp);
          if(memcmp(gold,mmi,sizeof(gold)) || memcmp(gold,vis,sizeof(gold))) {
            fprintf(stderr,"FAIL: bpp=%u width=%u sa=%u da=%u\n",bpp,width,sa,da);
            return 1;
          }
          ++cases;
        }
  printf("PASS packed-model: %lu cases, MMI=%lu VIS1=%lu vector groups emulated\n",
         cases,mmi_blocks,vis_blocks);
  return 0;
}
