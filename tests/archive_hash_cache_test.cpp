#include "archive_hash_cache.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#if MXL_ENABLE_DIAGNOSTICS
namespace mxl::diag {void note(const char*,int64_t) noexcept {}}
#endif
namespace {
using namespace mxl::archive_hash;
thread_local unsigned calls=0;
void check(bool value,const char* message){if(!value){fprintf(stderr,"FAIL %s\n",message);std::abort();}}
uint32_t __cdecl calculate(const char* path,uint32_t mode){++calls;uint32_t result=mode+1;while(*path)result=result*33+uint8_t(*path++);return result;}
__declspec(naked) void native_hash(){__asm {
    push [esp+4]
    push eax
    call calculate
    add esp,8
    mov ecx,0x1234abcd
    mov edx,0x87654321
    cmp eax,0
    ret 4
}}
constexpr uintptr_t Base=0x10000000;
constexpr uint32_t Returns[]={Base+0x26ad1,Base+0x26adc,Base+0x26ae9};
using Frame=std::array<uint32_t,11>;
Frame frame(const char* path,unsigned mode){return {0x71,0x72,0x73,0x74,0x75,0x76,0x77,uint32_t(path),0x202,Returns[mode%3],mode};}
Frame eligible(const char* path,unsigned mode){
    auto r=frame(path,mode);auto before=r;SetLastError(0x55667788);
    check(test_dispatch(r.data()),"eligible dispatch");check(GetLastError()==0x55667788,"last error");
    for(unsigned i=0;i<5;++i)check(r[i]==before[i],"nonvolatile registers and stack");
    check(r[5]==0x87654321 && r[6]==0x1234abcd,"native volatile outputs");
    check((r[8]&~0x8d5)==(before[8]&~0x8d5),"control flags");
    check(r[9]==before[9] && r[10]==before[10],"caller and stack argument");return r;
}
void rejected(Frame r){const auto before=r;check(!test_dispatch(r.data())&&r==before,"native fallback untouched");}
void run(){
    calls=0;take_stats();char name[]="DATA\\GLOBAL\\MONSTERS\\08\\TR\\08TRlitA1HTH.dcc";
    rejected(frame(name,0));
    {
        Scope outer(name);std::array<Frame,3> first;
        for(unsigned m=0;m<3;++m)first[m]=eligible(name,m);
        check(calls==3,"three native modes");
        for(unsigned repeat=0;repeat<10;++repeat)for(unsigned m=0;m<3;++m)check(eligible(name,m)==first[m],"identical cached registers and flags");
        check(calls==3,"bounded native calls per open");
        char duplicate[260]{};strcpy_s(duplicate,name);rejected(frame(duplicate,0));
        auto wrong=frame(name,0);wrong[9]++;rejected(wrong);wrong=frame(name,0);wrong[10]=3;rejected(wrong);
        name[0]='x';rejected(frame(name,0));name[0]='D';check(eligible(name,0)==first[0],"restored content");
        {Scope disabled(name,false);rejected(frame(name,0));}
        {Scope nested(duplicate);eligible(duplicate,0);rejected(frame(name,0));}
        check(eligible(name,0)==first[0],"outer scope restored");
        for(const char* invalid:std::array<const char*,4>{"","\tname",nullptr,reinterpret_cast<const char*>(1)}){Scope bad(invalid);rejected(frame(name,0));}
        char too_long[261];memset(too_long,'a',260);too_long[260]=0;{Scope bad(too_long);rejected(frame(too_long,0));}
        try {Scope nested(name);throw 1;}catch(int){}
        check(eligible(name,0)==first[0],"exception restores scope");
    }
    rejected(frame(name,0));
    auto stats=take_stats();check(stats.hits==33 && stats.native_calls==4 && calls==4,"exact counters");
    {Scope new_open(name);eligible(name,0);}check(calls==5,"no cross-open reuse");
    take_stats();
}
}
int main(){
    test_bind(reinterpret_cast<void*>(&native_hash),Base);run();
    std::thread one(run),two(run);one.join();two.join();
    puts("PASS native register/flag outputs, stack preservation, bounded modes, caller and input gates, nested/disabled/exception scopes, independent threads and no cross-open reuse");
}
