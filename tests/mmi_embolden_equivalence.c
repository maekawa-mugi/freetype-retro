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

static void sim_mmi_gray8_small(uint8_t *p, int pitch, int strength) {
  int x=pitch;
  while (x>0 && (((uintptr_t)(p+x))&15)) {
    unsigned total;
    --x;
    total=p[x];
    for(int k=1;k<=strength && k<=x;++k) total+=p[x-k];
    p[x]=(uint8_t)(total>255?255:total);
  }
  while (x>=32) {
    uint8_t *cur=p+x-16,*prev=cur-16,original[16],before[16],result[16];
    assert(!((uintptr_t)cur&15) && !((uintptr_t)prev&15));
    memcpy(original,cur,16);
    memcpy(before,prev,16);
    for(int i=0;i<16;++i) {
      unsigned total=original[i];
      for(int k=1;k<=strength;++k)
        total=sat8(total,i>=k?original[i-k]:before[16+i-k]);
      result[i]=(uint8_t)total;
    }
    memcpy(cur,result,16);
    ++total_vectors;
    x-=16;
  }
  while (x>0) {
    unsigned total;
    --x;
    total=p[x];
    for(int k=1;k<=strength && k<=x;++k) total+=p[x-k];
    p[x]=(uint8_t)(total>255?255:total);
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

static unsigned vis_vectors=0;
static void sim_vis1_or(uint8_t *dst, const uint8_t *src, int pitch) {
  int i=0;
  if ((((uintptr_t)dst)&7)==(((uintptr_t)src)&7)) {
    while(i<pitch && (((uintptr_t)(dst+i))&7)) {dst[i]|=src[i];++i;}
    for(;pitch-i>=8;i+=8) {
      for(int j=0;j<8;++j) dst[i+j]|=src[i+j];
      ++vis_vectors;
    }
  }
  while(i<pitch) {dst[i]|=src[i];++i;}
}

static unsigned total_tests=0;
static void compare(int pitch, int alignment, int ystr, int xstr, int numgrays, int negative) {
  /* Layout matches the FreeType positive/negative pitch traversal order.
   * Guard/padding bytes must be unchanged after each entire test case.
   */
  const size_t nrows = (size_t)5+ystr, total = 8192;
  uint8_t *a=malloc(total), *b=malloc(total), *c=malloc(total);
  assert(a && b && c);
  memset(a,0xA7,total);
  memset(b,0xA7,total);
  memset(c,0xA7,total);
  uint8_t *pa=a+128+alignment, *pb=b+128+alignment, *pc=c+128+alignment;
  assert(pitch <= 512 && nrows*(size_t)pitch + 160 < total);
  for(size_t i=0;i<nrows*(size_t)pitch;++i)
    pa[i]=pb[i]=pc[i]=(uint8_t)rnd();
  for(int row=0;row<5;++row) {
    int row_index = negative ? 4-row : row+ystr;
    int row_step = negative ? 1 : -1;
    uint8_t *r1=pa+row_index*pitch;
    uint8_t *r2=pb+row_index*pitch;
    uint8_t *r3=pc+row_index*pitch;
    reference_horizontal(r1,pitch,xstr,numgrays);
    if(xstr>=1 && xstr<=4 && numgrays==256) {
      if(xstr==1) sim_mmi_gray8_one(r2,pitch);
      else sim_mmi_gray8_small(r2,pitch,xstr);
    } else reference_horizontal(r2,pitch,xstr,numgrays);
    reference_horizontal(r3,pitch,xstr,numgrays);
    for(int k=1;k<=ystr;++k) {
      int offset=row_step*k*pitch;
      reference_or(r1+offset,r1,pitch);
      sim_mmi_or(r2+offset,r2,pitch);
      sim_vis1_or(r3+offset,r3,pitch);
    }
  }
  if(memcmp(a,b,total)!=0 || memcmp(a,c,total)!=0) {
    fprintf(stderr,"MISMATCH pitch=%d align=%d ystr=%d xstr=%d grays=%d negative=%d\n",
      pitch,alignment,ystr,xstr,numgrays,negative);
    abort();
  }
  free(a);free(b);free(c);
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
            compare(pitches[p],alignment,ystr,xstr,ngray,0);
            compare(pitches[p],alignment,ystr,xstr,ngray,1);
          }
  printf("PASS: %u cases, %u MMI 128-bit and %u VIS1 64-bit vector chunks emulated\n",
         total_tests,total_vectors,vis_vectors);
  return 0;
}
