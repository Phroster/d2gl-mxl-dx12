// SPDX-License-Identifier: GPL-3.0-or-later
// Execute the actual verified native projection fragment in this helper only.
// DONT_RESOLVE_DLL_REFERENCES skips game initialization; game files stay read-only.
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "world_objects.h"
#include "native_loot_pickup.h"
using namespace mxl::native_loot;
static void* projectionCode;
static int cameraX,cameraY,viewShift,resultX,resultY;
void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
__declspec(noinline) void invoke(int x,int y) {
    __asm {
        mov eax,x
        mov edx,y
        pushad
        sub esp,0d0h
        // CALL adds four bytes, matching the native [esp+0xc4] X slot.
        mov [esp+0c0h],eax
        mov edi,edx
        call projectionCode
        mov resultX,esi
        mov resultY,edi
        add esp,0d0h
        popad
    }
}
int main(int argc,char** argv) {
    HMODULE module=nullptr;
    try {
        check(argc==2,"expected D2Client.dll path");
        SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
        module=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        check(module!=nullptr,"cannot map native client without initialization");
        auto* client=reinterpret_cast<const uint8_t*>(module);
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(client);
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(client+dos->e_lfanew);
        check(nt->FileHeader.Machine==IMAGE_FILE_MACHINE_I386 && nt->OptionalHeader.SizeOfImage>0x11c420,"unsupported client image");
        const uint8_t entry[]={0x83,0xec,0x6c,0x56,0x57,0x89,0x54,0x24,0x08,0x8b,0xf1};
        check(!std::memcmp(client+0x6cc00,entry,sizeof(entry)),"world draw entry changed");
        int32_t displacement=0;std::memcpy(&displacement,client+0x6cd95,4);
        check(client[0x6cd94]==0xe8 && int32_t(0x6cd99)+displacement==0x6c490,"native object branch no longer enters the verified projection");
        std::array<uint8_t,43> expected={0x8b,0x35,0,0,0,0,0x8b,0x0d,0,0,0,0,0x8b,0x1d,0,0,0,0,
            0x8b,0x84,0x24,0xc4,0,0,0,0x2b,0xf1,0x2b,0xfb,0x03,0xf0,0x83,0xc7,0x08,
            0x89,0x74,0x24,0x18,0x89,0x7c,0x24,0x24,0xc3};
        const unsigned offsets[]={2,8,14},nativeOffsets[]={0x11c418,0x119960,0x11995c};
        for(unsigned i=0;i<3;++i) { const auto address=reinterpret_cast<uintptr_t>(client+nativeOffsets[i]);std::memcpy(expected.data()+offsets[i],&address,4); }
        check(!std::memcmp(client+0x6c4ec,expected.data(),42),"native camera projection differs from verified instructions");
        projectionCode=VirtualAlloc(nullptr,expected.size(),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        check(projectionCode!=nullptr,"cannot allocate isolated code fragment");
        std::memcpy(projectionCode,client+0x6c4ec,42);static_cast<uint8_t*>(projectionCode)[42]=0xc3;
        int* values[]={&viewShift,&cameraX,&cameraY};
        for(unsigned i=0;i<3;++i) { const auto address=reinterpret_cast<uintptr_t>(values[i]);std::memcpy(static_cast<uint8_t*>(projectionCode)+offsets[i],&address,4); }
        DWORD old=0;check(VirtualProtect(projectionCode,expected.size(),PAGE_EXECUTE_READ,&old)!=0,"cannot execute isolated fragment");
        FlushInstructionCache(GetCurrentProcess(),projectionCode,expected.size());
        unsigned visible=0,oldRejected=0;
        for(unsigned i=0;i<12000;++i) {
            cameraX=12000+int(i)*3;cameraY=8000+int(i)*2;
            viewShift=i%3==0?-256:i%3==1?256:0;
            const int x=cameraX+80+int(i%800),y=cameraY+80+int(i%500);
            invoke(x,y);
            const auto ours=object_screen_point(x,y,cameraX,cameraY,viewShift);
            check(ours.x==resultX && ours.y==resultY,"custom object anchor diverges from executed native projection");
            if(world_input_point(0,1024,768,ours.x,ours.y)) {
                ++visible;if(!world_input_point(0,1024,768,x,y)) ++oldRejected;
            }
        }
        check(visible>9000 && oldRejected==visible,"old missing-marker failure was not reproduced");
        std::printf("PASS: 12000 actual native object projections match; %u visible placements rejected by old world-space check.\n",oldRejected);
        VirtualFree(projectionCode,0,MEM_RELEASE);FreeLibrary(module);return 0;
    } catch(const std::exception& e) {
        if(projectionCode) VirtualFree(projectionCode,0,MEM_RELEASE);
        if(module) FreeLibrary(module);
        std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;
    }
}
