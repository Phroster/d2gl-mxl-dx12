/* Exercise the real verified Rust limiter in this isolated test process.
 * DONT_RESOLVE_DLL_REFERENCES skips DllMain and all game initialization. */
#define MXL_SMOOTHING_TEST
#define main smoothing_fixture_main
#include "../d2gl/d2gl/src/mxl_smoothing.c"
#undef main

static void *invoke_target;
static DWORD outputs[7];
static BYTE output_sse[128],input_sse[128];
static void invoke(void *target,uint64_t now,DWORD den,DWORD num) {
    DWORD now_low=(DWORD)now,now_high=(DWORD)(now>>32);invoke_target=target;
    __asm {
        lea eax,input_sse
        movdqu xmm0,[eax]
        movdqu xmm1,[eax+16]
        movdqu xmm2,[eax+32]
        movdqu xmm3,[eax+48]
        movdqu xmm4,[eax+64]
        movdqu xmm5,[eax+80]
        movdqu xmm6,[eax+96]
        movdqu xmm7,[eax+112]
        mov ecx,now_low
        mov edx,now_high
        // Save an aligned C frame base before substituting native sentinels.
        mov outputs[24],ebx
        push num
        push den
        mov ebx,33445566h
        mov edi,22334455h
        mov esi,11223344h
        call invoke_target
        lea esp,[esp+8]
        mov outputs[0],eax
        mov outputs[4],ecx
        mov outputs[8],edx
        mov outputs[12],ebx
        mov outputs[16],esi
        mov outputs[20],edi
        mov ebx,outputs[24]
        pushfd
        pop outputs[24]
        lea eax,output_sse
        movdqu [eax],xmm0
        movdqu [eax+16],xmm1
        movdqu [eax+32],xmm2
        movdqu [eax+48],xmm3
        movdqu [eax+64],xmm4
        movdqu [eax+80],xmm5
        movdqu [eax+96],xmm6
        movdqu [eax+112],xmm7
    }
}
#define REQUIRE(x) do{if(!(x)){printf("FAIL line %d at frame %u: %s\n",__LINE__,i,#x);return 1;}}while(0)
int main(int argc,char **argv) {
    unsigned i=0;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    if(argc!=2)return 2;
    HMODULE fps=LoadLibraryExA(argv[1],NULL,DONT_RESOLVE_DLL_REFERENCES);
    wchar_t filename[32768];
    if(!fps || !GetModuleFileNameW(fps,filename,32768) || !file_hash_matches(filename,"db9de4d4d320a7b70e66fe6b4aaa0e6f1560a5300a4993cc81cf4512ab1240c1"))return 3;
    epoch_fps=(BYTE *)fps;epoch_frequency=10000000;
    epoch_client=(BYTE *)VirtualAlloc(NULL,0x135000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!epoch_client)return 4;
    game_type=(DWORD *)(epoch_client+0x11c394);*(DWORD *)game_type=3;
    *(DWORD *)(epoch_client+0x11c310)=1;
    *(uint64_t *)(epoch_fps+0x38020)=epoch_frequency;
    memset(epoch_fps+0x38050,0,48);
    render_update_original=epoch_fps+0x11900;
    BytePatch patch=render_patch(epoch_fps),bad=patch;bad.expected[4]^=1;
    REQUIRE(!apply_patches(&bad,1));REQUIRE(!memcmp(patch.instruction,patch.expected,5));
    REQUIRE(apply_patches(&patch,1));
    // Resolve and call the target encoded by the actual installed patch, with
    // exactly the original caller-cleaned arguments. Do not run draw_game.
    DWORD displacement;memcpy(&displacement,patch.instruction+1,4);
    void *patched_target=(void *)((uintptr_t)(patch.instruction+5)+displacement);
    REQUIRE(patched_target==update_render_time);
    BYTE native_state[48],before[48],baseline_sse[128];DWORD baseline[7];
    memcpy(native_state,epoch_fps+0x38050,48);
    for(unsigned n=0;n<128;++n)input_sse[n]=(BYTE)(n*37+11);
    uint64_t now=0xffff0000ULL,previous=0;unsigned skipped=0,drawn=0;
    for(i=0;i<12000;++i) {
        unsigned num=i<6000?144:120;
        now+=i%19==0?1000:(num==144?69480:83400);
        if(i==3000)now+=500000; // genuine load gap
        if(i==9000)*(DWORD *)(epoch_client+0x11c310)=40;
        if(i==10000)*(DWORD *)game_type=0;
        if(i==11000)*(DWORD *)game_type=3;
        memcpy(before,native_state,48);memcpy(epoch_fps+0x38050,before,48);
        invoke(render_update_original,now,1,num);
        memcpy(native_state,epoch_fps+0x38050,48);memcpy(baseline,outputs,sizeof(baseline));memcpy(baseline_sse,output_sse,128);
        memcpy(epoch_fps+0x38050,before,48);
        if(render_clock.active)*(uint64_t *)(epoch_fps+0x38070)=render_clock.visual;
        invoke(patched_target,now,1,num);
        REQUIRE(!memcmp(epoch_fps+0x38050,native_state,32));
        REQUIRE(!memcmp(epoch_fps+0x38078,native_state+40,8));
        REQUIRE((outputs[0]&255)==(baseline[0]&255));
        REQUIRE(outputs[12/4]==0x33445566 && outputs[16/4]==0x11223344 && outputs[20/4]==0x22334455);
        REQUIRE(((outputs[6]^baseline[6])&0xcd5)==0);
        REQUIRE(!memcmp(output_sse,baseline_sse,128));
        if(!(outputs[0]&255)){++skipped;if(render_clock.active)REQUIRE(render_clock.visual==previous);}
        else {++drawn;previous=*(uint64_t *)(epoch_fps+0x38070);}
        if(*game_type!=3)REQUIRE(*(uint64_t *)(epoch_fps+0x38070)==*(uint64_t *)(native_state+32));
    }
    REQUIRE(skipped>50 && drawn>10000);
    printf("PASS: real native limiter %u draws/%u skipped; deadlines, counts, rates, SSE, flags, nonvolatile registers, gap/area resets, SP and return value unchanged.\n",drawn,skipped);
    return 0;
}
