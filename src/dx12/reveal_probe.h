#pragma once
#include <windows.h>
#include <cstdint>
namespace mxl::diag {
using RevealRootFn=uintptr_t(__cdecl*)();
using RevealNodeFn=uintptr_t(__fastcall*)(void*);
using RevealInitFn=uintptr_t(__stdcall*)(void*);
using RevealRoomDataFn=uintptr_t(__stdcall*)(void*,uint32_t,uint32_t,uint32_t,void*);
bool start_reveal_probe(HWND window=nullptr);
void reveal_event(uint64_t trace,const char* phase,uint64_t began,uint64_t ended,int32_t act,int32_t level,int32_t x,int32_t y,bool resident) noexcept;
#ifdef MXL_REVEAL_TEST
bool test_reveal_probe(RevealRootFn root,RevealNodeFn level,RevealNodeFn room,RevealInitFn init,RevealRoomDataFn load,RevealRoomDataFn unload);
#endif
}
