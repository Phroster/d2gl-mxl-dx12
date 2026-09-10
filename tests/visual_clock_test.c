#include "mxl_visual_clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) {printf("FAIL line %d: %s\n",__LINE__,#x);return 1;} } while(0)

int main(int argc,char **argv) {
    MxlVisualClock s={0};
    const uint64_t freq=10000000,base=100000000000ULL,step=400000;
    if(argc==2) {
        FILE *f=fopen(argv[1],"r");if(!f)return 2;
        unsigned long long raw,render,frequency;unsigned ms,updates,level,type;
        puts("raw,epoch,reason,resets");
        while(fscanf(f,"%llu,%llu,%llu,%u,%u,%u,%u",&raw,&render,&frequency,&ms,&updates,&level,&type)==7) {
            uint64_t epoch=mxl_visual_epoch(&s,raw,render,frequency,ms,updates,level,type);
            printf("%llu,%llu,%u,%u\n",raw,(unsigned long long)epoch,s.reason,s.resets);
        }
        int ok=feof(f);fclose(f);return ok?0:3;
    }
    CHECK(mxl_visual_epoch(&s,base,base,freq,1000,100,1,3)==base && s.reason==MXL_EPOCH_ANCHOR);
    /* 100 minutes of alternating rounding/fallback error must not accumulate
     * or alter a native step. Includes jitter beyond the measured 1 ms cases. */
    for(unsigned i=1;i<=150000;++i) {
        uint64_t expected=base+i*step,raw=expected+(i%3==0?17591:i%3==1?10000:-10000LL);
        CHECK(mxl_visual_epoch(&s,raw,expected,freq,1000+i*40,100+i,1,3)==expected);
        CHECK(s.reason==MXL_EPOCH_STEP && s.resets==1);
    }
    uint64_t raw=s.epoch+step;
    CHECK(mxl_visual_epoch(&s,raw,raw,freq,s.update_ms+40,s.updates+1,2,3)==raw && s.reason==MXL_EPOCH_AREA);
    raw+=step;
    CHECK(mxl_visual_epoch(&s,raw,raw,freq,s.update_ms+40,s.updates,2,3)==raw && s.reason==MXL_EPOCH_GAP);
    raw+=step*10;
    CHECK(mxl_visual_epoch(&s,raw,raw,freq,s.update_ms+400,s.updates+10,2,3)==raw && s.reason==MXL_EPOCH_GAP);
    raw+=step;
    CHECK(mxl_visual_epoch(&s,raw,raw,freq,10,0,2,3)==raw && s.reason==MXL_EPOCH_GAP);
    raw+=step;
    CHECK(mxl_visual_epoch(&s,raw,raw,freq,50,1,2,0)==raw && !s.active && s.reason==MXL_EPOCH_OFF);
    CHECK(mxl_visual_epoch(&s,raw,raw,freq,50,1,2,3)==raw && s.reason==MXL_EPOCH_ANCHOR);
    CHECK(mxl_visual_epoch(&s,raw+step+210000,raw+step,freq,90,2,2,3)==raw+step+210000 && s.reason==MXL_EPOCH_DRIFT);
    CHECK(mxl_visual_epoch(&s,raw,raw,freq,130,3,2,3)==raw && s.reason==MXL_EPOCH_GAP);
    memset(&s,0,sizeof(s));
    CHECK(mxl_visual_epoch(&s,base,base,freq,0xfffffff0u,0xffffffffu,1,3)==base);
    CHECK(mxl_visual_epoch(&s,base+step+10000,base+step,freq,24,0,1,3)==base+step && s.reason==MXL_EPOCH_STEP);
    CHECK(mxl_visual_epoch(&s,base+step*6,base+step*6,freq,224,5,1,3)==base+step*6 && s.reason==MXL_EPOCH_STEP);
    CHECK(mxl_visual_epoch(&s,base,base,999,264,6,1,3)==base && !s.active);
    CHECK(mxl_visual_epoch(&s,base,base,10000001,264,6,1,3)==base && !s.active);
    CHECK(mxl_visual_epoch(&s,base,base,freq,264,6,0,3)==base && !s.active);
    for(unsigned type=0;type<10;++type) if(type!=3)
        CHECK(mxl_visual_epoch(&s,base,base,freq,1000,100,1,type)==base && !s.active);
    puts("PASS: visual clock rounding/fallback jitter, 100-minute stability, area/pause/session resets, catch-up, wrap, drift and non-realm preservation.");
    return 0;
}
