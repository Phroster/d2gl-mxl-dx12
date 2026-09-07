#include "reveal_probe.h"
#include "reveal_signatures.h"
#include "diagnostics.h"
#include "auto_reveal.h"
#include <detours/detours.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <array>
#include <atomic>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

namespace mxl::diag {
namespace {
RevealRootFn original_root=nullptr;
RevealNodeFn original_level=nullptr,original_room=nullptr;
RevealInitFn original_init=nullptr;
RevealRoomDataFn original_load=nullptr,original_unload=nullptr;
RevealNodeFn original_preset=nullptr;
RevealBuildAreaFn original_build_area=nullptr;
RevealInitFn original_prepare_room=nullptr;
PVOID original_dt1=nullptr,original_tile_grid=nullptr;
RevealLookupFn original_lookup=nullptr;
RevealLayerFn original_layer=nullptr;
uintptr_t sigma_base=0;
bool probe_ready=false;
struct Context {uint64_t id;int32_t act;};
thread_local Context* active=nullptr;
std::atomic<uint64_t> sequence{0};
uint32_t read32(uintptr_t address) noexcept {if(!address)return 0;__try{return *reinterpret_cast<uint32_t*>(address);}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
struct Node {int32_t level=-1,x=-1,y=-1;bool resident=false;};
Node room_info(void* pointer) {
    const auto room=reinterpret_cast<uintptr_t>(pointer);Node node;
    if(room){const auto level=read32(room+0x58);if(level)node.level=int32_t(read32(level+0x1d0));
        node.x=int32_t(read32(room+0x34));node.y=int32_t(read32(room+0x38));node.resident=read32(room+0x30)!=0;}
    return node;
}
void emit(const char* phase,uint64_t began,const Node& node={}) noexcept {
    if(active)reveal_event(active->id,phase,began,ticks(),active->act,node.level,node.x,node.y,node.resident);
}
void record_automap_callback(void* level) noexcept {
    const auto misc=read32(reinterpret_cast<uintptr_t>(level)+0x1b4);
    const auto callback=read32(misc?misc+0x454:0);
    if(!callback)return;
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(callback),&module))return;
    char path[MAX_PATH]{},message[96]{};
    if(!GetModuleFileNameA(module,path,MAX_PATH))return;
    const auto name=strrchr(path,'\\');
    _snprintf_s(message,sizeof(message),_TRUNCATE,"preset_automap_callback_%.48s",name?name+1:path);
    note(message,callback-reinterpret_cast<uintptr_t>(module));
}
uintptr_t __cdecl root_hook() {
    const auto incoming_error=GetLastError();
    if(!enabled()){SetLastError(incoming_error);return original_root();}
    auto* previous=active;Context context{++sequence,-1};
    if(sigma_base){const auto player=read32(read32(sigma_base+0x3fa130));const auto act=read32(player?player+0x1c:0);if(act)context.act=int32_t(read32(act+0x14));}
    active=&context;const auto began=ticks();uintptr_t result=0;DWORD error=0;
    __try {SetLastError(incoming_error);result=original_root();error=GetLastError();emit("act",began);}
    __finally {active=previous;}
    SetLastError(error);return result;
}
uintptr_t __fastcall level_hook(void* level) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return original_level(level);}
    Node node;node.level=int32_t(read32(reinterpret_cast<uintptr_t>(level)+0x1d0));
    const auto began=ticks();SetLastError(incoming_error);const auto result=original_level(level);const auto error=GetLastError();emit("level_rooms",began,node);SetLastError(error);return result;
}
uintptr_t __fastcall room_hook(void* room) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return original_room(room);}
    const auto node=room_info(room);const auto began=ticks();SetLastError(incoming_error);const auto result=original_room(room);const auto error=GetLastError();emit("room",began,node);SetLastError(error);return result;
}
uintptr_t __stdcall init_hook(void* level) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return original_init(level);}
    Node node;node.level=int32_t(read32(reinterpret_cast<uintptr_t>(level)+0x1d0));
    const auto began=ticks();SetLastError(incoming_error);const auto result=original_init(level);const auto error=GetLastError();emit("level_generation",began,node);SetLastError(error);return result;
}
uintptr_t __stdcall load_hook(void* act,uint32_t level,uint32_t x,uint32_t y,void* room) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return original_load(act,level,x,y,room);}
    Node node{int32_t(level),int32_t(x),int32_t(y),false};const auto began=ticks();
    SetLastError(incoming_error);const auto result=original_load(act,level,x,y,room);const auto error=GetLastError();emit("load_room",began,node);SetLastError(error);return result;
}
uintptr_t __stdcall unload_hook(void* act,uint32_t level,uint32_t x,uint32_t y,void* room) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return original_unload(act,level,x,y,room);}
    Node node{int32_t(level),int32_t(x),int32_t(y),false};const auto began=ticks();
    SetLastError(incoming_error);const auto result=original_unload(act,level,x,y,room);const auto error=GetLastError();emit("unload_room",began,node);SetLastError(error);return result;
}
uintptr_t __fastcall preset_hook(void* level) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return original_preset(level);}
    Node node;node.level=int32_t(read32(reinterpret_cast<uintptr_t>(level)+0x1d0));
    record_automap_callback(level);
    const auto began=ticks();SetLastError(incoming_error);const auto result=original_preset(level);const auto error=GetLastError();emit("preset_generation",began,node);SetLastError(error);return result;
}
uintptr_t __stdcall build_area_hook(void* level,void* map,uint32_t flags,uint32_t single_room) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return original_build_area(level,map,flags,single_room);}
    Node node;node.level=int32_t(read32(reinterpret_cast<uintptr_t>(level)+0x1d0));
    const auto began=ticks();SetLastError(incoming_error);const auto result=original_build_area(level,map,flags,single_room);const auto error=GetLastError();emit("preset_build_area",began,node);SetLastError(error);return result;
}
uintptr_t __stdcall prepare_room_hook(void* room) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return original_prepare_room(room);}
    const auto node=room_info(room);const auto began=ticks();SetLastError(incoming_error);const auto result=original_prepare_room(room);const auto error=GetLastError();emit("preset_room_prepare",began,node);SetLastError(error);return result;
}
// These entrypoints have no stack arguments. Convert only the verified register
// arguments; use an ordinary call/return chain so nested calls and exceptions do
// not depend on a substituted return address or a thread-local return stack.
__declspec(naked) uintptr_t __stdcall call_dt1(void* room) {
    __asm {
        push esi
        mov esi, dword ptr [esp+8]
        call dword ptr [original_dt1]
        pop esi
        ret 4
    }
}
__declspec(naked) uintptr_t __stdcall call_tile_grid(void* room,void* pool) {
    __asm {
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [esp+8]
        call dword ptr [original_tile_grid]
        ret 8
    }
}
uintptr_t __stdcall dt1_wrapper(void* room) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return call_dt1(room);}
    const auto node=room_info(room);const auto began=ticks();SetLastError(incoming_error);const auto result=call_dt1(room);const auto error=GetLastError();emit("dt1_load",began,node);SetLastError(error);return result;
}
uintptr_t __stdcall tile_grid_wrapper(void* room,void* pool) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return call_tile_grid(room,pool);}
    const auto node=room_info(room);const auto began=ticks();SetLastError(incoming_error);const auto result=call_tile_grid(room,pool);const auto error=GetLastError();emit("room_tile_grid",began,node);SetLastError(error);return result;
}
__declspec(naked) uintptr_t dt1_hook() {
    __asm {
        push esi
        call dt1_wrapper
        ret
    }
}
__declspec(naked) uintptr_t tile_grid_hook() {
    __asm {
        push ecx
        push eax
        call tile_grid_wrapper
        ret
    }
}
uintptr_t __fastcall lookup_hook(void* misc,uint32_t level) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return original_lookup(misc,level);}
    Node node;node.level=int32_t(level);const auto began=ticks();SetLastError(incoming_error);const auto result=original_lookup(misc,level);const auto error=GetLastError();emit("level_lookup",began,node);SetLastError(error);return result;
}
uintptr_t __fastcall layer_hook(uint32_t layer) {
    const auto incoming_error=GetLastError();
    if(!active || !enabled()){SetLastError(incoming_error);return original_layer(layer);}
    // Layer IDs are not level IDs: use x for this phase, leaving level unknown.
    Node node;node.x=int32_t(layer);const auto began=ticks();SetLastError(incoming_error);const auto result=original_layer(layer);const auto error=GetLastError();emit("automap_layer",began,node);SetLastError(error);return result;
}
bool attach() {
    std::vector<HANDLE> threads;threads.reserve(512);LONG error=DetourTransactionBegin();
    if(error!=NO_ERROR){note("reveal_hook_transaction_failed",error);return false;}
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if(snapshot==INVALID_HANDLE_VALUE)error=GetLastError();
    else {
        THREADENTRY32 entry{};entry.dwSize=sizeof(entry);BOOL found=Thread32First(snapshot,&entry);
        if(!found)error=GetLastError();
        while(found && error==NO_ERROR){
            if(entry.th32OwnerProcessID==GetCurrentProcessId() && entry.th32ThreadID!=GetCurrentThreadId()){
                if(threads.size()==threads.capacity()){error=ERROR_TOO_MANY_TCBS;break;}
                HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
                if(thread){threads.push_back(thread);error=DetourUpdateThread(thread);}
                else if(GetLastError()!=ERROR_INVALID_PARAMETER)error=GetLastError();
            }
            found=Thread32Next(snapshot,&entry);
        }
        CloseHandle(snapshot);
    }
    struct Hook {PVOID* original;PVOID replacement;};
    const Hook hooks[]={{reinterpret_cast<PVOID*>(&original_root),reinterpret_cast<PVOID>(&root_hook)},
        {reinterpret_cast<PVOID*>(&original_level),reinterpret_cast<PVOID>(&level_hook)},
        {reinterpret_cast<PVOID*>(&original_room),reinterpret_cast<PVOID>(&room_hook)},
        {reinterpret_cast<PVOID*>(&original_init),reinterpret_cast<PVOID>(&init_hook)},
        {reinterpret_cast<PVOID*>(&original_load),reinterpret_cast<PVOID>(&load_hook)},
        {reinterpret_cast<PVOID*>(&original_unload),reinterpret_cast<PVOID>(&unload_hook)},
        {reinterpret_cast<PVOID*>(&original_preset),reinterpret_cast<PVOID>(&preset_hook)},
        {reinterpret_cast<PVOID*>(&original_build_area),reinterpret_cast<PVOID>(&build_area_hook)},
        {reinterpret_cast<PVOID*>(&original_prepare_room),reinterpret_cast<PVOID>(&prepare_room_hook)},
        {&original_dt1,reinterpret_cast<PVOID>(&dt1_hook)},
        {&original_tile_grid,reinterpret_cast<PVOID>(&tile_grid_hook)},
        {reinterpret_cast<PVOID*>(&original_lookup),reinterpret_cast<PVOID>(&lookup_hook)},
        {reinterpret_cast<PVOID*>(&original_layer),reinterpret_cast<PVOID>(&layer_hook)}};
    for(const auto& hook:hooks)if(error==NO_ERROR)error=DetourAttach(hook.original,hook.replacement);
    if(error==NO_ERROR)error=DetourTransactionCommit();else DetourTransactionAbort();
    for(auto thread:threads)CloseHandle(thread);
    note(error==NO_ERROR?"reveal_thirteen_hooks_ready":"reveal_hooks_unavailable",error);return error==NO_ERROR;
}
bool file_hash(HMODULE module,const char* expected) {
    wchar_t path[32768]{};if(!GetModuleFileNameW(module,path,32768))return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool good=false;
    if(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) && CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        std::array<BYTE,65536> buffer{};DWORD count=0;bool complete=true;
        for(;;){if(!ReadFile(file,buffer.data(),DWORD(buffer.size()),&count,nullptr)){complete=false;break;}if(!count)break;if(!CryptHashData(hash,buffer.data(),count,0)){complete=false;break;}}
        BYTE digest[32]{};DWORD size=sizeof(digest);
        if(complete && CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)){
            char text[65]{};for(size_t i=0;i<32;++i)sprintf_s(text+i*2,3,"%02x",digest[i]);good=!strcmp(text,expected);
        }
    }
    if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);return good;
}
bool signature(uintptr_t base,const RevealSite& site) noexcept {
    __try {for(size_t i=0;i<32;++i)if(site.mask[i] && reinterpret_cast<const uint8_t*>(base+site.rva)[i]!=site.bytes[i])return false;return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
bool start_reveal_probe(HWND window) {
    static bool attempted=false;if(attempted)return probe_ready;attempted=true;
    wchar_t executable[32768]{};
    if(!GetModuleFileNameW(nullptr,executable,32768))return false;
    const auto filename=wcsrchr(executable,L'\\');
    if(_wcsicmp(filename?filename+1:executable,L"Game.exe"))return false;
    const auto sigma=GetModuleHandleW(L"D2Sigma.dll"),common=GetModuleHandleW(L"D2Common.dll");
    if(!sigma || !common){note("reveal_modules_unavailable");return false;}
    if(!file_hash(sigma,"ff44257078d994809d6b1a5a3a28657a75b1bb75395b1ee0714361d78355b728") ||
       !file_hash(common,"59fa5928522f566f2bf99675571206ad70df889c89d3d07fa87edf5083e06e10")){
        note("reveal_unsupported_file_identity");return false;
    }
    const auto sb=reinterpret_cast<uintptr_t>(sigma),cb=reinterpret_cast<uintptr_t>(common);
    for(size_t i=0;i<6;++i)if(!signature(i<3?sb:cb,reveal_sites[i])){note("reveal_signature_mismatch",i);return false;}
    if(read32(sb+0x3de0f6)!=sb+0x1f7be8 || read32(sb+0x3de0fa)!=sb+0x68ab0){note("reveal_binding_changed");return false;}
    // Both renderers contain this code. Only one owns act-entry reveal, including
    // when logging is off and its separate diagnostic mutex was never created.
    const auto owner_name=L"Local\\MXLActEntryReveal-"+std::to_wstring(GetCurrentProcessId());
    HANDLE owner=CreateMutexW(nullptr,FALSE,owner_name.c_str());
    if(!owner)return false;
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(owner);return false;}
    HMODULE self=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&start_reveal_probe),&self)){CloseHandle(owner);return false;}
    // Retain the ownership handle for the process lifetime, like the callbacks.
    original_root=reinterpret_cast<RevealRootFn>(sb+reveal_sites[0].rva);original_level=reinterpret_cast<RevealNodeFn>(sb+reveal_sites[1].rva);
    original_room=reinterpret_cast<RevealNodeFn>(sb+reveal_sites[2].rva);original_init=reinterpret_cast<RevealInitFn>(cb+reveal_sites[3].rva);
    original_load=reinterpret_cast<RevealRoomDataFn>(cb+reveal_sites[4].rva);original_unload=reinterpret_cast<RevealRoomDataFn>(cb+reveal_sites[5].rva);
    original_preset=reinterpret_cast<RevealNodeFn>(cb+reveal_sites[6].rva);
    original_build_area=reinterpret_cast<RevealBuildAreaFn>(cb+reveal_sites[7].rva);
    original_prepare_room=reinterpret_cast<RevealInitFn>(cb+reveal_sites[8].rva);
    original_dt1=reinterpret_cast<PVOID>(cb+reveal_sites[9].rva);
    original_tile_grid=reinterpret_cast<PVOID>(cb+reveal_sites[10].rva);
    original_lookup=reinterpret_cast<RevealLookupFn>(cb+reveal_sites[11].rva);
    original_layer=reinterpret_cast<RevealLayerFn>(sb+reveal_sites[12].rva);
    sigma_base=sb;
    // No diagnostic detours or thread suspension when recording is disabled.
    // If attachment fails, the all-or-nothing transaction preserves the direct
    // original entrypoint, which is still suitable for automatic reveal.
    if(enabled()){
        bool deep_signatures_match=true;
        for(size_t i=6;i<13;++i)if(!signature(i==12?sb:cb,reveal_sites[i])){
            note("reveal_deep_signature_mismatch",i);deep_signatures_match=false;break;
        }
        if(deep_signatures_match)attach();
    }
    probe_ready=window && mxl::reveal::start(window,sb,&root_hook);
    if(!probe_ready)note("auto_reveal_start_failed");
    return probe_ready;
}
#ifdef MXL_REVEAL_TEST
bool test_reveal_probe(RevealRootFn root,RevealNodeFn level,RevealNodeFn room,RevealInitFn init,RevealRoomDataFn load,RevealRoomDataFn unload,const RevealDeepFns& deep) {
    original_root=root;original_level=level;original_room=room;original_init=init;original_load=load;original_unload=unload;
    original_preset=deep.preset;original_build_area=deep.build_area;original_prepare_room=deep.prepare_room;
    original_dt1=deep.dt1;original_tile_grid=deep.tile_grid;original_lookup=deep.lookup;original_layer=deep.layer;
    return attach();
}
#endif
}
