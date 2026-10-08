/* Host-side functional emulator for the R5900 embolden fast path.
 * The MMI LQ/QFSRV/PADDUB/SQ sequence is modeled byte-for-byte.
 * Does not execute R5900 machine instructions.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rng_state = 0x13579bdf;
static uint32_t rnd(void) {
  uint32_t x = rng_state;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  return rng_state = x;
}

static void reference_horizontal(uint8_t *p, int pitch, int xstr, int num_grays) {
  for (int x = pitch - 1; x >= 0; --x) {
    for (int i = 1; i <= xstr; ++i) {
      if (x - i < 0) break;
      if ((unsigned)p[x] + p[x-i] > (unsigned)(num_grays - 1)) {
        p[x] = (uint8_t)(num_grays - 1);
        break;
      }
      p[x] = (uint8_t)(p[x] + p[x-i]);
      if (p[x] == num_grays-1) break;
    }
  }
}

static uint8_t sat8(unsigned a, unsigned b) {
  unsigned v = a + b;
  return (uint8_t)(v > 255 ? 255 : v);
}

static unsigned total_vectors = 0;
static void sim_mmi_gray8_one(uint8_t *p, int pitch) {
  int x = pitch;
  while (x>0 && (((uintptr_t)(p+x)) & 15)) {
    --x;
    p[x] = sat8(p[x], x>0 ? p[x-1]:0);
  }
  while (x >= 32) {
    uint8_t *current = p+x-16, *previous = current-16;
    uint8_t curr[16], prev[16], shifted[16], output[16];
    assert(!((uintptr_t)current & 15));
    assert(!((uintptr_t)previous & 15));
    memcpy(curr,current,16);
    memcpy(prev,previous,16);
    /* qfsrv(result, current, previous) shifted by 15 bytes right */
    shifted[0]=prev[15];
    for (int i=1;i<16;++i) shifted[i]=curr[i-1];
    for (int i=0;i<16;++i) output[i]=sat8(curr[i],shifted[i]);
    memcpy(current,output,16);
    ++total_vectors;
    x-=16;
  }
  while(x>0) {
    --x;
    p[x] = sat8(p[x],x>0 ? p[x-1]:0);
  }
}

static void reference_or(uint8_t* dst, const uint8_t* src, int pitch) {
  for(int i=0;i<pitch;++i) dst[i] |= src[i];
}
static void sim_mmi_or(uint8_t* dst, const uint8_t* src, int pitch) {
  int i=0;
  if ((((uintptr_t)dst)&15)==(((uintptr_t)src)&15)) {
    while(i<pitch && (((uintptr_t)(dst+i)) & 15)) {dst[i]|=src[i];++i;}
    for (;pitch-i>=16;i+=16) {
      for(int j=0;j<16;++j) dst[i+j]|=src[i+j];
      ++total_vectors;
    }
  }
  while(i<pitch) {dst[i]|=src[i];++i;}
}

static unsigned total_tests=0;
static void compare(int pitch, int alignment, int ystr, int xstr, int numgrays) {
  /* Use oversized row-stride and guard regions. */
  const size_t nrows = 5 + ystr, total = 8192;
  uint8_t *a=malloc(total), *b=malloc(total);
  assert(a && b);
  memset(a,0xA7,total);
  memset(b,0xA7,total);
  uint8_t *pa=a+128+alignment, *pb=b+128+alignment;
  assert(pitch <= 512 && (size_t)(nrows*pitch) + 160 < total);
  for(size_t i=0;i<nrows*(size_t)pitch;++i) pa[i]=pb[i]=(uint8_t)rnd();
  for(int row=0;row<5;++row) {
    uint8_t *r1=pa + (row+ystr)*pitch;
    uint8_t *r2=pb + (row+ystr)*pitch;
    reference_horizontal(r1,pitch,xstr,numgrays);
    if (xstr==1 && numgrays==256) sim_mmi_gray8_one(r2,pitch);
    else reference_horizontal(r2,pitch,xstr,numgrays);
    for(int k=1;k<=ystr;++k) {
      reference_or(r1-k*pitch,r1,pitch);
      sim_mmi_or(r2-k*pitch,r2,pitch);
    }
  }
  if(memcmp(a,b,total)!=0) {
    fprintf(stderr,"MISMATCH pitch=%d align=%d ystr=%d xstr=%d numgrays=%d\n",pitch,alignment,ystr,xstr,numgrays);
    abort();
  }
  free(a);free(b);
  ++total_tests;
}
int main(void) {
  const int pitches[]={1,2,3,4,7,8,14,15,16,17,31,32,33,47,48,49,63,64,65,95,96,97,127,128,129,255,256,257,511,512};
  for(unsigned p=0;p<sizeof(pitches)/sizeof(pitches[0]);++p)
    for(int alignment=0;alignment<16;++alignment)
      for(int ystr=0;ystr<=4;++ystr)
        for(int xstr=0;xstr<=4;++xstr)
          for(int gray_sel=0;gray_sel<3;++gray_sel) {
            int ngray=gray_sel==0?256:gray_sel==1?16:2;
            compare(pitches[p],alignment,ystr,xstr,ngray);
          }
  printf("PASS: %u deterministic end-to-end cases; %u 128-bit vector chunks emulated\n",total_tests,total_vectors);
  return 0;
}
