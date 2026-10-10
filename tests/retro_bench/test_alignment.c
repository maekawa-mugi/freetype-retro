/* Host regression for correctness checks leaking their final offset
 * into timed runs. Build separately from bench.c, which is included.
 * cc -O2 -std=c99 tests/retro_bench/test_alignment.c -o alignment-test
 */
#define main rb_benchmark_main
#include "bench.c"
#undef main

int main(void)
{
  unsigned offset,kind;
  const unsigned kinds[]={GRAYFILL,BGRA,MONO,LCD_V_NEG};
  for(kind=0;kind<sizeof(kinds)/sizeof(kinds[0]);kind++)
    for(offset=0;offset<16;offset++){
      struct rb_case t={"alignment-regression",0,64,1,0,0};
      unsigned before=16U+offset;
      t.kind=(int)kinds[kind];
      t.offset=offset;
      rb_offset=before;
      if(!validate_case(&t,0) || rb_offset!=before){
        fprintf(stderr,"FAIL correctness offset leaked: kind=%u offset=%u\n",
                kinds[kind],offset);
        return 1;
      }
    }
  puts("PASS correctness checks preserve all 16 timed alignments");
  return 0;
}
