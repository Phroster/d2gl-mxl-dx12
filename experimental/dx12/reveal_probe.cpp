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
uintptr_t __cdecl root_hook() {
    if(!enabled())return original_root();
    auto* previous=active;Context context{++sequence,-1};
    if(sigma_base){const auto player=read32(read32(sigma_base+0x3fa130));const auto act=read32(player?player+0x1c:0);if(act)context.act=int32_t(read32(act+0x14));}
    active=&context;const auto began=ticks();uintptr_t result=0;DWORD error=0;
    __try {result=original_root();error=GetLastError();emit("act",began);}
    __finally {active=previous;}
    SetLastError(error);return result;
}
uintptr_t __fastcall level_hook(void* level) {
    if(!active || !enabled())return original_level(level);
    Node node;node.level=int32_t(read32(reinterpret_cast<uintptr_t>(level)+0x1d0));
    const auto began=ticks();const auto result=original_level(level);const auto error=GetLastError();emit("level_rooms",began,node);SetLastError(error);return result;
}
uintptr_t __fastcall room_hook(void* room) {
    if(!active || !enabled())return original_room(room);
    const auto node=room_info(room);const auto began=ticks();const auto result=original_room(room);const auto error=GetLastError();emit("room",began,node);SetLastError(error);return result;
}
uintptr_t __stdcall init_hook(void* level) {
    if(!active || !enabled())return original_init(level);
    Node node;node.level=int32_t(read32(reinterpret_cast<uintptr_t>(level)+0x1d0));
    const auto began=ticks();const auto result=original_init(level);const auto error=GetLastError();emit("level_generation",began,node);SetLastError(error);return result;
}
uintptr_t __stdcall load_hook(void* act,uint32_t level,uint32_t x,uint32_t y,void* room) {
    if(!active || !enabled())return original_load(act,level,x,y,room);
    Node node{int32_t(level),int32_t(x),int32_t(y),false};const auto began=ticks();
    const auto result=original_load(act,level,x,y,room);const auto error=GetLastError();emit("load_room",began,node);SetLastError(error);return result;
}
uintptr_t __stdcall unload_hook(void* act,uint32_t level,uint32_t x,uint32_t y,void* room) {
    if(!active || !enabled())return original_unload(act,level,x,y,room);
    Node node{int32_t(level),int32_t(x),int32_t(y),false};const auto began=ticks();
    const auto result=original_unload(act,level,x,y,room);const auto error=GetLastError();emit("unload_room",began,node);SetLastError(error);return result;
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
        {reinterpret_cast<PVOID*>(&original_unload),reinterpret_cast<PVOID>(&unload_hook)}};
    for(const auto& hook:hooks)if(error==NO_ERROR)error=DetourAttach(hook.original,hook.replacement);
    if(error==NO_ERROR)error=DetourTransactionCommit();else DetourTransactionAbort();
    for(auto thread:threads)CloseHandle(thread);
    note(error==NO_ERROR?"reveal_six_hooks_ready":"reveal_hooks_unavailable",error);return error==NO_ERROR;
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
    static bool attempted=false;if(attempted)return probe_ready;attempted=true;if(!enabled())return false;
    const auto sigma=GetModuleHandleW(L"D2Sigma.dll"),common=GetModuleHandleW(L"D2Common.dll");
    if(!sigma || !common){note("reveal_modules_unavailable");return false;}
    if(!file_hash(sigma,"ff44257078d994809d6b1a5a3a28657a75b1bb75395b1ee0714361d78355b728") ||
       !file_hash(common,"59fa5928522f566f2bf99675571206ad70df889c89d3d07fa87edf5083e06e10")){
        note("reveal_unsupported_file_identity");return false;
    }
    const auto sb=reinterpret_cast<uintptr_t>(sigma),cb=reinterpret_cast<uintptr_t>(common);
    for(size_t i=0;i<6;++i)if(!signature(i<3?sb:cb,reveal_sites[i])){note("reveal_signature_mismatch",i);return false;}
    if(read32(sb+0x3de0f6)!=sb+0x1f7be8 || read32(sb+0x3de0fa)!=sb+0x68ab0){note("reveal_binding_changed");return false;}
    original_root=reinterpret_cast<RevealRootFn>(sb+reveal_sites[0].rva);original_level=reinterpret_cast<RevealNodeFn>(sb+reveal_sites[1].rva);
    original_room=reinterpret_cast<RevealNodeFn>(sb+reveal_sites[2].rva);original_init=reinterpret_cast<RevealInitFn>(cb+reveal_sites[3].rva);
    original_load=reinterpret_cast<RevealRoomDataFn>(cb+reveal_sites[4].rva);original_unload=reinterpret_cast<RevealRoomDataFn>(cb+reveal_sites[5].rva);
    sigma_base=sb;probe_ready=attach();
    if(probe_ready && window && !mxl::reveal::start(window,sb,&root_hook))note("auto_reveal_start_failed");
    return probe_ready;
}
#ifdef MXL_REVEAL_TEST
bool test_reveal_probe(RevealRootFn root,RevealNodeFn level,RevealNodeFn room,RevealInitFn init,RevealRoomDataFn load,RevealRoomDataFn unload) {
    original_root=root;original_level=level;original_room=room;original_init=init;original_load=load;original_unload=unload;return attach();
}
#endif
}
