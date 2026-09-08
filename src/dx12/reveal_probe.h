#pragma once
#include <windows.h>
#include <cstdint>
namespace mxl::diag {
using RevealRootFn=uintptr_t(__cdecl*)();
using RevealNodeFn=uintptr_t(__fastcall*)(void*);
using RevealInitFn=uintptr_t(__stdcall*)(void*);
using RevealRoomDataFn=uintptr_t(__stdcall*)(void*,uint32_t,uint32_t,uint32_t,void*);
using RevealBuildAreaFn=uintptr_t(__stdcall*)(void*,void*,uint32_t,uint32_t);
using RevealLookupFn=uintptr_t(__fastcall*)(void*,uint32_t);
using RevealLayerFn=uintptr_t(__fastcall*)(uint32_t);
// The two raw entrypoints use game-specific register arguments, never a C ABI.
struct RevealDeepFns {
    RevealNodeFn preset=nullptr;
    RevealBuildAreaFn build_area=nullptr;
    RevealInitFn prepare_room=nullptr;
    void* dt1=nullptr;
    void* tile_grid=nullptr;
    RevealLookupFn lookup=nullptr;
    RevealLayerFn layer=nullptr;
};
bool start_reveal_probe(HWND window=nullptr);
void reveal_event(uint64_t trace,const char* phase,uint64_t began,uint64_t ended,int32_t act,int32_t level,int32_t x,int32_t y,bool resident) noexcept;
#ifdef MXL_REVEAL_TEST
bool test_reveal_lookup_signature(uintptr_t base);
bool test_reveal_before_scene_signature(uintptr_t base,uintptr_t fps=0);
// Disabled by default; the test supplies a real assembly caller's return label
// after placing the supported ordering/layer signatures in a synthetic image.
bool test_reveal_preselection(uintptr_t sigma,uintptr_t caller);
bool test_reveal_probe(RevealRootFn root,RevealNodeFn level,RevealNodeFn room,RevealInitFn init,RevealRoomDataFn load,RevealRoomDataFn unload,const RevealDeepFns& deep,uint32_t sites=(1u<<13)-1);
#endif
}
