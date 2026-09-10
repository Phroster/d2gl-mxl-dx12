#include "mxl_render_clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv) {
    MxlRenderClock s={0};
    const uint64_t f=10000000,base=100000000000ULL;
    if(argc==2) {
        FILE *file=fopen(argv[1],"r");if(!file)return 2;
        unsigned long long raw,now,freq;unsigned den,num,level,type;
        puts("raw,visual,resets,active");
        while(fscanf(file,"%llu,%llu,%llu,%u,%u,%u,%u",&raw,&now,&freq,&den,&num,&level,&type)==7) {
            uint64_t v=mxl_render_time(&s,raw,now,freq,den,num,level,type);
            printf("%llu,%llu,%u,%u\n",raw,(unsigned long long)v,s.resets,s.active);
        }
        int ok=feof(file);fclose(file);return ok?0:3;
    }
    uint64_t previous=0;unsigned skips=0;uint64_t priorraw=0;
    // Actual cadence is slightly slower than nominal, with sub-ms CPU jitter.
    // Native floor slots occasionally skip. Visual steps must remain continuous.
    for(unsigned i=0;i<100000;++i) {
        uint64_t now=base+(uint64_t)i*69480+(i%101==50?6000:0);
        uint64_t raw=(now*144/f)*f/144;
        uint64_t v=mxl_render_time(&s,raw,now,f,1,144,1,3);
        if(i) {
            if(raw-priorraw>100000)++skips;
            CHECK(v>previous && v-previous>68000 && v-previous<71000);
            CHECK(v<now+10000 && now<v+80000);
        }
        previous=v;priorraw=raw;
    }
    CHECK(skips>50 && s.resets==1);
    CHECK(mxl_render_time(&s,base,base,f,1,144,1,0)==base && !s.active);
    CHECK(mxl_render_time(&s,base,base,f,0,144,1,3)==base && !s.active);
    CHECK(mxl_render_time(&s,base,base,f,1,0,1,3)==base && !s.active);
    CHECK(mxl_render_time(&s,base,base,f,1,1000000001,1,3)==base && !s.active);
    CHECK(mxl_render_time(&s,base,base,UINT64_MAX,1,144,1,3)==base && !s.active);
    memset(&s,0,sizeof(s));mxl_render_time(&s,base,base,f,1,144,1,3);
    CHECK(mxl_render_time(&s,base+500000,base+500000,f,1,144,1,3)==base+500000 && s.resets==2);
    CHECK(mxl_render_time(&s,base+570000,base+570000,f,1,144,2,3)==base+570000 && s.resets==3);
    CHECK(mxl_render_time(&s,base+640000,base+640000,f,1,120,2,3)==base+640000 && s.resets==4);
    CHECK(mxl_render_time(&s,base+600000,base+600000,f,1,120,2,3)==base+600000 && s.resets==5);
    // Fractional refresh and long uptime, including a low-word carry.
    memset(&s,0,sizeof(s));previous=0;
    for(unsigned i=0;i<50000;++i) {
        uint64_t now=0xffff0000ULL+((uint64_t)i*f*1001)/144000;
        uint64_t v=mxl_render_time(&s,now,now,f,1001,144000,1,3);
        if(i)CHECK(v>previous && v-previous>=69513 && v-previous<=69515);
        previous=v;
    }
    CHECK(s.resets==1);
    puts("PASS: continuous render steps, fractional refresh, jitter, long uptime, carry, rate/area/gap resets and native passthrough.");
    return 0;
}
