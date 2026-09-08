#include "sound_cancel.h"
#include "sound_cancel_guards.h"
#include "sound_probe.h"
#include "diagnostics.h"
#include <intrin.h>
#include <atomic>
#include <wincrypt.h>
#include <array>
#include <cstdio>
#include <cstring>

namespace mxl::sound_cancel {
namespace {
Api original;
uintptr_t free_return=0,cancel_return=0;
bool installed=false;
std::atomic<bool> wake_enabled{false};
thread_local bool sound_release=false;

// The client discards an unused sound's async job. Fog cancels its reads, then
// waits for completion before releasing any buffer, handle or job allocation.
// Keep that lifetime and wait intact. Only wake Storm after its native cancel
// has removed the requests from the pending list and queued their completions.
void cancel(void* buffer,uintptr_t caller) {
    const bool wake=sound_release && buffer && caller==cancel_return && wake_enabled.load(std::memory_order_relaxed);
    original.cancel(buffer);const auto error=GetLastError();
    if(wake) {
        // With the matching requests already removed, the verified priority
        // helper finds nothing to reprioritize. Its unconditional SetEvent
        // wakes the existing worker to dispatch cancellation completion now.
        original.prioritize(buffer,1);
        mxl::diag::note("sound_cancel_wakeup",1);
    }
    SetLastError(error);
}
void invoke_release(void* job,bool previous) {
    __try {original.release(job);}
    __finally {sound_release=previous;}
}
void release(void* job,uintptr_t caller) {
    const auto incoming=GetLastError();const auto began=mxl::diag::audio_enabled()?mxl::diag::ticks():0;
    const bool previous=sound_release;
    // An unrelated nested release hides the outer scope, including when it
    // unwinds through a native structured exception.
    sound_release=caller==free_return && wake_enabled.load(std::memory_order_relaxed);
    SetLastError(incoming);invoke_release(job,previous);const auto error=GetLastError();
    if(began)mxl::diag::native_sound_call(mxl::diag::NativeSound::AsyncFree,began,mxl::diag::ticks(),caller,reinterpret_cast<uintptr_t>(job),0,0);
    SetLastError(error);
}
void __fastcall release_hook(void* job){const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());release(job,caller);}
void WINAPI cancel_hook(void* buffer){const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());cancel(buffer,caller);}

template<size_t N,size_t R> bool code_is(uintptr_t module,uintptr_t rva,uintptr_t preferred,
    const std::array<uint8_t,N>& bytes,const std::array<size_t,R>& relocs) noexcept {
    auto expected=bytes;
    for(auto offset:relocs){uint32_t value;memcpy(&value,expected.data()+offset,4);value+=uint32_t(module-preferred);memcpy(expected.data()+offset,&value,4);}
    __try {return !memcmp(reinterpret_cast<const void*>(module+rva),expected.data(),N);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool code_ready(uintptr_t client,uintptr_t fog,uintptr_t storm) noexcept {
    using namespace guard_data;
#define GUARD(module,name) code_is(module,name##Rva,name##ImageBase,name##Bytes,name##Relocs)
    return GUARD(client,ClientFreeTail) && GUARD(fog,FogFreeWait) && GUARD(fog,FogCancelThunk) &&
        GUARD(storm,StormCancel) && GUARD(storm,StormPrioritize) && GUARD(storm,StormPriorityList);
#undef GUARD
}
bool file_hash(HMODULE module,const char* expected) {
    wchar_t path[32768]{};if(!GetModuleFileNameW(module,path,32768))return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool good=false;
    if(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) && CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)) {
        std::array<BYTE,32768> bytes{};DWORD count=0;bool complete=true;
        for(;;){if(!ReadFile(file,bytes.data(),DWORD(bytes.size()),&count,nullptr)){complete=false;break;}if(!count)break;if(!CryptHashData(hash,bytes.data(),count,0)){complete=false;break;}}
        BYTE digest[32]{};DWORD size=sizeof(digest);
        if(complete && CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)) {
            char text[65]{};for(size_t i=0;i<32;++i)sprintf_s(text+i*2,3,"%02x",digest[i]);good=!strcmp(text,expected);
        }
    }
    if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);return good;
}
void* read_slot(void** slot) noexcept {__try{return slot?*slot:nullptr;}__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}}
bool exchange(void** slot,void* expected,void* replacement) noexcept {
    DWORD protection=0;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection))return false;
    const auto previous=InterlockedCompareExchangePointer(slot,replacement,expected);DWORD ignored=0;
    if(!VirtualProtect(slot,sizeof(void*),protection,&ignored))mxl::diag::note("sound_cancel_protection_restore_failed",GetLastError());
    return previous==expected;
}
bool install(void** free_slot,void** cancel_slot,Api api,uintptr_t free_caller,uintptr_t cancel_caller,bool enabled) {
    if(installed)return true;
    if(!api.release || !api.cancel || !api.prioritize || !free_caller || !cancel_caller || free_slot==cancel_slot ||
        reinterpret_cast<uintptr_t>(free_slot)%alignof(void*) || reinterpret_cast<uintptr_t>(cancel_slot)%alignof(void*) ||
        read_slot(free_slot)!=reinterpret_cast<void*>(api.release) || read_slot(cancel_slot)!=reinterpret_cast<void*>(api.cancel))return false;
    original=api;free_return=free_caller;cancel_return=cancel_caller;
    // Publish the cancellation hook before the client can enter an eligible
    // scope. Calls from other Fog users keep the original cancel path.
    if(!exchange(cancel_slot,reinterpret_cast<void*>(api.cancel),reinterpret_cast<void*>(&cancel_hook)))return false;
    if(!exchange(free_slot,reinterpret_cast<void*>(api.release),reinterpret_cast<void*>(&release_hook))) {
        if(!exchange(cancel_slot,reinterpret_cast<void*>(&cancel_hook),reinterpret_cast<void*>(api.cancel)))mxl::diag::note("sound_cancel_rollback_failed");
        return false;
    }
    wake_enabled.store(enabled,std::memory_order_release);installed=true;return true;
}
}
bool start() {
    if(installed)return true;
    wchar_t executable[32768]{};if(!GetModuleFileNameW(nullptr,executable,32768))return false;
    auto* filename=wcsrchr(executable,L'\\');if(!filename || _wcsicmp(filename+1,L"Game.exe"))return false;
    wcscpy_s(filename+1,32768-(filename+1-executable),L"d2gl.ini");
    const bool requested=GetPrivateProfileIntW(L"Other",L"sound_cancel_wakeup",1,executable)!=0;
    if(!requested && !mxl::diag::audio_enabled())return false;
    const auto client=GetModuleHandleW(L"D2Client.dll"),fog=GetModuleHandleW(L"Fog.dll"),storm=GetModuleHandleW(L"Storm.dll");
    if(!client || !fog || !storm ||
        !file_hash(client,"dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906") ||
        !file_hash(fog,"53f015869c495c760d2c5a6d8d836c8b5f0f5437b69ca0dbf0d977dfa5ec96cf") ||
        !file_hash(storm,"a4f31ef82f49dbf1af206e23072aa1401a2ef7f99cb9dc794d5fa59c519290ef")) {
        mxl::diag::note("sound_cancel_identity_unavailable");return false;
    }
    const auto cb=reinterpret_cast<uintptr_t>(client),fb=reinterpret_cast<uintptr_t>(fog),sb=reinterpret_cast<uintptr_t>(storm);
    if(!code_ready(cb,fb,sb)){mxl::diag::note("sound_cancel_code_unavailable");return false;}
    Api api{reinterpret_cast<FreeFn>(GetProcAddress(fog,MAKEINTRESOURCEA(10097))),
        reinterpret_cast<CancelFn>(GetProcAddress(storm,MAKEINTRESOURCEA(283))),
        reinterpret_cast<PriorityFn>(GetProcAddress(storm,MAKEINTRESOURCEA(282)))};
    const bool ready=install(reinterpret_cast<void**>(cb+0xcedbc),reinterpret_cast<void**>(fb+0x25214),api,cb+0x5dddb,fb+0x1e0bd,requested);
    mxl::diag::note(ready?"sound_cancel_imports_ready":"sound_cancel_imports_unavailable",ready?2:0);
    mxl::diag::note("sound_cancel_fix_ready",ready && requested?1:0);return ready;
}
#ifdef MXL_SOUND_CANCEL_TEST
bool test_install(void** a,void** b,Api api,uintptr_t fc,uintptr_t cc,bool enabled){return install(a,b,api,fc,cc,enabled);}
void test_release(void* job,uintptr_t caller){release(job,caller);}
void test_cancel(void* buffer,uintptr_t caller){cancel(buffer,caller);}
void test_enabled(bool enabled){wake_enabled.store(enabled);}
bool test_code_ready(uintptr_t client,uintptr_t fog,uintptr_t storm){return code_ready(client,fog,storm);}
#endif
}
