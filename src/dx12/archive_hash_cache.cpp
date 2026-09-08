#include "archive_hash_cache.h"
#include "diagnostics.h"
#include <detours/detours.h>
#include <tlhelp32.h>
#include <array>
#include <vector>
#include <cstring>

namespace mxl::archive_hash {
namespace {
void* original_hash=nullptr;
uintptr_t lookup_returns[3]{};
thread_local Scope* current=nullptr;
thread_local Stats counters{};
bool installed=false;
// PUSHFD/PUSHAD, followed by the original return address and stack argument.
struct Registers {uint32_t edi,esi,ebp,esp,ebx,edx,ecx,eax,flags,caller,mode;};
static_assert(sizeof(Registers)==44);
bool snapshot(const char* path,char* out,uint32_t& length) noexcept {
    __try {
        if(!path)return false;
        for(uint32_t i=0;i<260;++i){
            auto c=static_cast<unsigned char>(path[i]);out[i]=char(c);
            if(!c){length=i;return i!=0;}
            // CRT locale/extended-character behavior remains on the native path.
            if(c<32 || c>126)return false;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}
bool same(const char* path,const char* saved,uint32_t bytes) noexcept {
    __try {return memcmp(path,saved,bytes)==0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Storm uses EAX for the name, one callee-popped stack argument for hash mode.
// Capture all native volatile register and arithmetic-flag outputs, not only
// EAX. Nonvolatile registers and the caller's stack remain untouched.
__declspec(naked) void __cdecl invoke(Result* out,const char* path,uint32_t mode) {
    __asm {
        push ebx
        push ebp
        push esi
        push edi
        mov esi,[esp+20]
        mov eax,[esp+24]
        push [esp+28]
        call original_hash
        pushfd
        pop dword ptr [esi+12]
        mov [esi],eax
        mov [esi+4],ecx
        mov [esi+8],edx
        pop edi
        pop esi
        pop ebp
        pop ebx
        ret
    }
}
}
bool dispatch(void* saved) noexcept {
    auto& r=*static_cast<Registers*>(saved);auto* scope=current;
    if(!scope || !scope->active_ || r.mode>2 ||
        r.caller!=lookup_returns[r.mode] ||
        reinterpret_cast<const char*>(r.eax)!=scope->input_ ||
        !same(scope->input_,scope->path_,scope->length_+1))return false;
    auto& value=scope->values_[r.mode];
    if(!scope->valid_[r.mode]){
        invoke(&value,scope->input_,r.mode);scope->valid_[r.mode]=true;++counters.native_calls;
    }else ++counters.hits;
    r.eax=value.eax;r.ecx=value.ecx;r.edx=value.edx;
    constexpr uint32_t arithmetic=0x8d5; // CF/PF/AF/ZF/SF/OF; retain control flags.
    r.flags=(r.flags&~arithmetic)|(value.flags&arithmetic);
    return true;
}
namespace {
__declspec(naked) void hook() {
    __asm {
        pushfd
        pushad
        push esp
        call dispatch
        add esp,4
        test al,al
        jz native_path
        popad
        popfd
        ret 4
    native_path:
        popad
        popfd
        jmp original_hash
    }
}
bool code_ready(uintptr_t base) noexcept {
    // Full hash body, including its relocated crypt-table pointer operand.
    std::array<uint8_t,86> expected{
        0x53,0x55,0x8b,0x6c,0x24,0x0c,0x56,0x8b,0xd8,0x85,0xdb,0x57,0xbe,0xed,0x7f,0xed,0x7f,
        0xbf,0xee,0xee,0xee,0xee,0x74,0x35,0x8a,0x03,0x84,0xc0,0x74,0x2f,0x0f,0xbe,0xc0,0x50,
        0xe8,0x62,0x07,0xfe,0xff,0x8b,0x15,0,0,0,0,0x03,0xf7,0x6b,0xff,0x21,0x0f,0xbe,0xc0,
        0x8b,0xcd,0xc1,0xe1,0x08,0x03,0xc8,0x33,0x34,0x8a,0x83,0xc4,0x04,0x43,0x03,0xf8,
        0x85,0xdb,0x8d,0x7c,0x37,0x03,0x75,0xcb,0x5f,0x8b,0xc6,0x5e,0x5d,0x5b,0xc2,0x04,0x00};
    const uint32_t table=uint32_t(base+0x53120);memcpy(expected.data()+41,&table,4);
    constexpr std::array<uint8_t,41> lookup{
        0x83,0xec,0x0c,0x55,0x56,0x57,0x6a,0x00,0x8b,0xf0,0x8b,0xf9,0xe8,0xff,0xf3,0xff,0xff,
        0x8b,0xe8,0x6a,0x01,0x8b,0xc6,0xe8,0xf4,0xf3,0xff,0xff,0x89,0x44,0x24,0x10,0x6a,0x02,
        0x8b,0xc6,0xe8,0xe7,0xf3,0xff,0xff};
    __try {return !memcmp(reinterpret_cast<void*>(base+0x25ed0),expected.data(),expected.size()) &&
        !memcmp(reinterpret_cast<void*>(base+0x26ac0),lookup.data(),lookup.size());}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
Scope::Scope(const char* path,bool enabled) noexcept {
    previous_=current;input_=path;
    active_=enabled && snapshot(path,path_,length_);
    // Even an ineligible nested open temporarily hides the outer scope.
    current=this;
}
void Scope::leave() noexcept {if(entered_){current=previous_;entered_=false;}}
Scope::~Scope(){leave();}
bool start(HMODULE storm) {
    if(installed)return true;
    const auto base=reinterpret_cast<uintptr_t>(storm);
    if(!storm || !code_ready(base))return false;
    std::vector<HANDLE> threads;threads.reserve(512);
    auto error=DetourTransactionBegin();
    if(error!=NO_ERROR)return false;
    const auto snapshot_handle=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if(snapshot_handle==INVALID_HANDLE_VALUE)error=GetLastError();
    else {
        THREADENTRY32 entry{};entry.dwSize=sizeof(entry);BOOL found=Thread32First(snapshot_handle,&entry);
        if(!found)error=GetLastError();
        while(found && error==NO_ERROR){
            if(entry.th32OwnerProcessID==GetCurrentProcessId() && entry.th32ThreadID!=GetCurrentThreadId()){
                if(threads.size()==threads.capacity()){error=ERROR_TOO_MANY_TCBS;break;}
                auto thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
                if(thread){threads.push_back(thread);error=DetourUpdateThread(thread);}
                else if(GetLastError()!=ERROR_INVALID_PARAMETER)error=GetLastError();
            }
            found=Thread32Next(snapshot_handle,&entry);
        }
        CloseHandle(snapshot_handle);
    }
    original_hash=reinterpret_cast<void*>(base+0x25ed0);
    lookup_returns[0]=base+0x26ad1;lookup_returns[1]=base+0x26adc;lookup_returns[2]=base+0x26ae9;
    if(error==NO_ERROR)error=DetourAttach(&original_hash,reinterpret_cast<void*>(&hook));
    if(error==NO_ERROR)error=DetourTransactionCommit();else DetourTransactionAbort();
    for(auto thread:threads)CloseHandle(thread);
    installed=error==NO_ERROR;
    if(!installed)diag::note("archive_hash_install_failed",error);
    return installed;
}
Stats take_stats(){const auto result=counters;counters={};return result;}
void flush_stats(){const auto stats=take_stats();if(stats.hits){diag::note("archive_hash_hits",stats.hits);diag::note("archive_hash_native_calls",stats.native_calls);}}
#ifdef MXL_ARCHIVE_HASH_TEST
void test_bind(void* native,uintptr_t base){original_hash=native;lookup_returns[0]=base+0x26ad1;lookup_returns[1]=base+0x26adc;lookup_returns[2]=base+0x26ae9;}
bool test_dispatch(uint32_t* registers){return dispatch(registers);}
bool test_code_ready(uintptr_t base){return code_ready(base);}
#endif
}
