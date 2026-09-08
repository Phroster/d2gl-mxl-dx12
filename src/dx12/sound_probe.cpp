#include "sound_probe.h"
#include "diagnostics.h"
#include <intrin.h>
#include <wincrypt.h>
#include <array>
#include <cstdio>
#include <cstring>

namespace mxl::diag {
namespace {
using LoadFn=void*(__fastcall*)(void*,const char*,BOOL,LONG,int,void*,void*,int,const char*,int);
using BufferFn=void*(__fastcall*)(void*);
using ReadyFn=BOOL(__fastcall*)(void*);
using OpenFn=uint32_t(__fastcall*)(const char*,void**);
using ReadFn=uint32_t(__fastcall*)(void*,void*,uint32_t,uint32_t*,uint32_t,uint32_t,uint32_t);
using CloseFn=uint32_t(__fastcall*)(void*);
using SectionFn=void(WINAPI*)(LPCRITICAL_SECTION);
using WaitFn=DWORD(WINAPI*)(HANDLE,DWORD);
using SleepFn=void(WINAPI*)(DWORD);
using MusicBeginFn=int(WINAPI*)(HANDLE,int,unsigned,DWORD,int,int,int);
using MusicEndFn=BOOL(WINAPI*)(HANDLE);
using MusicPositionFn=BOOL(WINAPI*)(HANDLE,void*,void*);
LoadFn load_original=nullptr; BufferFn buffer_original=nullptr; ReadyFn ready_original=nullptr;
OpenFn open_original=nullptr; ReadFn read_original=nullptr; CloseFn close_original=nullptr;
SectionFn enter_original=nullptr,leave_original=nullptr;
WaitFn wait_original[2]{}; SleepFn sleep_original[2]{};
MusicBeginFn music_begin_original=nullptr; MusicEndFn music_end_original=nullptr;
MusicPositionFn music_position_original=nullptr;
void* locks[2]{};bool installed=false;
struct Held {unsigned depth=0;uint64_t began=0;uintptr_t caller=0;};
thread_local Held held[2];
int lock_index(void* lock) {for(int i=0;i<2;++i)if(locks[i]==lock)return i;return -1;}
uint64_t begin() {return audio_enabled()?ticks():0;}
void finish(NativeSound op,uint64_t began,uintptr_t caller,uintptr_t object,uint32_t argument,uintptr_t result,
    const char* path="",unsigned status=3) {
    if(began)native_sound_call(op,began,ticks(),caller,object,argument,result,path,status);
}
unsigned snapshot(const char* path,char (&out)[96]) noexcept {
    __try {
        if(!path)return 2;
        for(unsigned i=0;i<95;++i){out[i]=path[i];if(!out[i])return 0;}
        out[95]=0;return path[95]?1:0;
    }__except(EXCEPTION_EXECUTE_HANDLER){out[0]=0;return 2;}
}
uintptr_t output_handle(void** output) noexcept {
    __try {return output?reinterpret_cast<uintptr_t>(*output):0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
void* __fastcall load_hook(void* pool,const char* path,BOOL async,LONG offset,int size,void* buffer,
    void* callback,int priority,const char* source,int line) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto began=begin();char name[96]{};const auto status=began?snapshot(path,name):3;
    SetLastError(incoming);auto* result=load_original(pool,path,async,offset,size,buffer,callback,priority,source,line);
    const auto error=GetLastError();finish(NativeSound::AsyncLoad,began,caller,reinterpret_cast<uintptr_t>(result),uint32_t(priority),reinterpret_cast<uintptr_t>(result),name,status);
    SetLastError(error);return result;
}
void* __fastcall buffer_hook(void* job) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    SetLastError(incoming);auto* result=buffer_original(job);const auto error=GetLastError();
    finish(NativeSound::AsyncBuffer,began,caller,reinterpret_cast<uintptr_t>(job),0,reinterpret_cast<uintptr_t>(result));SetLastError(error);return result;
}
BOOL __fastcall ready_hook(void* job) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    SetLastError(incoming);const auto result=ready_original(job);const auto error=GetLastError();
    finish(NativeSound::AsyncReady,began,caller,reinterpret_cast<uintptr_t>(job),0,result);SetLastError(error);return result;
}
uint32_t __fastcall open_hook(const char* path,void** output) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    char name[96]{};const auto status=began?snapshot(path,name):3;
    SetLastError(incoming);const auto result=open_original(path,output);const auto error=GetLastError();
    finish(NativeSound::ClientOpen,began,caller,result?output_handle(output):0,0,result,name,status);SetLastError(error);return result;
}
uint32_t __fastcall read_hook(void* handle,void* target,uint32_t requested,uint32_t* completed,uint32_t a5,uint32_t a6,uint32_t a7) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    SetLastError(incoming);const auto result=read_original(handle,target,requested,completed,a5,a6,a7);const auto error=GetLastError();
    finish(NativeSound::ClientRead,began,caller,reinterpret_cast<uintptr_t>(handle),requested,result);SetLastError(error);return result;
}
uint32_t __fastcall close_hook(void* handle) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    SetLastError(incoming);const auto result=close_original(handle);const auto error=GetLastError();
    finish(NativeSound::ClientClose,began,caller,reinterpret_cast<uintptr_t>(handle),0,result);SetLastError(error);return result;
}
void WINAPI enter_hook(LPCRITICAL_SECTION lock) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    SetLastError(incoming);enter_original(lock);const auto error=GetLastError();const auto ended=began?ticks():0;
    if(began) {
        const auto index=lock_index(lock);
        if(index>=0){auto& h=held[index];if(h.depth++==0){h.began=ended;h.caller=caller;}}
        native_sound_call(NativeSound::LockWait,began,ended,caller,reinterpret_cast<uintptr_t>(lock),0,0);
    }
    SetLastError(error);
}
void WINAPI leave_hook(LPCRITICAL_SECTION lock) {
    const auto incoming=GetLastError();const auto index=lock_index(lock);Held completed{};
    if(index>=0){auto& h=held[index];if(h.depth && --h.depth==0){completed=h;h={};}}
    const auto ended=completed.began?ticks():0;
    SetLastError(incoming);leave_original(lock);const auto error=GetLastError();
    if(completed.began)native_sound_call(NativeSound::LockHold,completed.began,ended,completed.caller,reinterpret_cast<uintptr_t>(lock),0,0);
    SetLastError(error);
}
template<unsigned Source> DWORD WINAPI wait_hook(HANDLE handle,DWORD timeout) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    SetLastError(incoming);const auto result=wait_original[Source](handle,timeout);const auto error=GetLastError();
    finish(Source?NativeSound::ClientWait:NativeSound::SoundWait,began,caller,reinterpret_cast<uintptr_t>(handle),timeout,result);SetLastError(error);return result;
}
template<unsigned Source> void WINAPI sleep_hook(DWORD duration) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    SetLastError(incoming);sleep_original[Source](duration);const auto error=GetLastError();
    finish(Source?NativeSound::ClientSleep:NativeSound::SoundSleep,began,caller,0,duration,0);SetLastError(error);
}
int WINAPI music_begin_hook(HANDLE file,int a2,unsigned a3,DWORD offset,int a5,int a6,int a7) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    SetLastError(incoming);const auto result=music_begin_original(file,a2,a3,offset,a5,a6,a7);const auto error=GetLastError();
    finish(NativeSound::MusicBegin,began,caller,reinterpret_cast<uintptr_t>(file),offset,result);SetLastError(error);return result;
}
BOOL WINAPI music_end_hook(HANDLE file) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    SetLastError(incoming);const auto result=music_end_original(file);const auto error=GetLastError();
    finish(NativeSound::MusicEnd,began,caller,reinterpret_cast<uintptr_t>(file),0,result);SetLastError(error);return result;
}
BOOL WINAPI music_position_hook(HANDLE file,void* position,void* length) {
    const auto incoming=GetLastError();const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const auto began=begin();
    SetLastError(incoming);const auto result=music_position_original(file,position,length);const auto error=GetLastError();
    finish(NativeSound::MusicPosition,began,caller,reinterpret_cast<uintptr_t>(file),0,result);SetLastError(error);return result;
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
    if(!VirtualProtect(slot,sizeof(void*),protection,&ignored))note("native_sound_protection_restore_failed",GetLastError());
    return previous==expected;
}
bool install(void*** slots,void** originals,void* first_lock,void* second_lock) {
    if(installed)return true;
    if(!slots || !originals || !first_lock || !second_lock || first_lock==second_lock)return false;
    void* replacements[]={reinterpret_cast<void*>(&load_hook),reinterpret_cast<void*>(&buffer_hook),reinterpret_cast<void*>(&ready_hook),
        reinterpret_cast<void*>(&open_hook),reinterpret_cast<void*>(&read_hook),reinterpret_cast<void*>(&close_hook),
        reinterpret_cast<void*>(&enter_hook),reinterpret_cast<void*>(&leave_hook),reinterpret_cast<void*>(&wait_hook<0>),reinterpret_cast<void*>(&sleep_hook<0>),
        reinterpret_cast<void*>(&music_begin_hook),reinterpret_cast<void*>(&music_end_hook),reinterpret_cast<void*>(&music_position_hook),
        reinterpret_cast<void*>(&wait_hook<1>),reinterpret_cast<void*>(&sleep_hook<1>)};
    for(size_t i=0;i<std::size(replacements);++i) {
        if(!originals[i] || reinterpret_cast<uintptr_t>(slots[i])%alignof(void*) || read_slot(slots[i])!=originals[i])return false;
        for(size_t j=0;j<i;++j)if(slots[j]==slots[i])return false;
    }
    load_original=reinterpret_cast<LoadFn>(originals[0]);buffer_original=reinterpret_cast<BufferFn>(originals[1]);ready_original=reinterpret_cast<ReadyFn>(originals[2]);
    open_original=reinterpret_cast<OpenFn>(originals[3]);read_original=reinterpret_cast<ReadFn>(originals[4]);close_original=reinterpret_cast<CloseFn>(originals[5]);
    enter_original=reinterpret_cast<SectionFn>(originals[6]);leave_original=reinterpret_cast<SectionFn>(originals[7]);
    wait_original[0]=reinterpret_cast<WaitFn>(originals[8]);sleep_original[0]=reinterpret_cast<SleepFn>(originals[9]);
    music_begin_original=reinterpret_cast<MusicBeginFn>(originals[10]);music_end_original=reinterpret_cast<MusicEndFn>(originals[11]);music_position_original=reinterpret_cast<MusicPositionFn>(originals[12]);
    wait_original[1]=reinterpret_cast<WaitFn>(originals[13]);sleep_original[1]=reinterpret_cast<SleepFn>(originals[14]);locks[0]=first_lock;locks[1]=second_lock;
    size_t changed=0;for(;changed<std::size(replacements);++changed)if(!exchange(slots[changed],originals[changed],replacements[changed]))break;
    if(changed!=std::size(replacements)) {
        while(changed){--changed;if(!exchange(slots[changed],replacements[changed],originals[changed]))note("native_sound_rollback_failed",changed);}
        return false;
    }
    installed=true;return true;
}
}
bool start_sound_probe() {
    if(installed)return true;if(!audio_enabled())return false;
    const auto client=GetModuleHandleW(L"D2Client.dll"),sound=GetModuleHandleW(L"D2sound.dll"),fog=GetModuleHandleW(L"Fog.dll"),storm=GetModuleHandleW(L"Storm.dll"),kernel=GetModuleHandleW(L"kernel32.dll");
    if(!client || !sound || !fog || !storm || !kernel ||
        !file_hash(client,"dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906") ||
        !file_hash(sound,"b24dfb0159c76867e7f3d14602e73bccc37d59b7c8d2def88213b01084b61a72") ||
        !file_hash(fog,"53f015869c495c760d2c5a6d8d836c8b5f0f5437b69ca0dbf0d977dfa5ec96cf") ||
        !file_hash(storm,"a4f31ef82f49dbf1af206e23072aa1401a2ef7f99cb9dc794d5fa59c519290ef")) {
        note("native_sound_identity_unavailable");return false;
    }
    const auto cb=reinterpret_cast<uintptr_t>(client),sb=reinterpret_cast<uintptr_t>(sound);
    const uintptr_t addresses[]={cb+0xcedac,cb+0xcedb8,cb+0xcedb0,cb+0xceda8,cb+0xcee54,cb+0xced9c,
        sb+0xf11c,sb+0xf120,sb+0xf118,sb+0xf0c4,sb+0xf16c,sb+0xf168,sb+0xf164,cb+0xcefb0,cb+0xcefa0};
    void** slots[15]{};for(unsigned i=0;i<15;++i)slots[i]=reinterpret_cast<void**>(addresses[i]);
    const auto ordinal=[](HMODULE module,WORD id){return reinterpret_cast<void*>(GetProcAddress(module,MAKEINTRESOURCEA(id)));};
    void* originals[]={ordinal(fog,10091),ordinal(fog,10094),ordinal(fog,10092),ordinal(fog,10102),ordinal(fog,10104),ordinal(fog,10103),
        reinterpret_cast<void*>(GetProcAddress(kernel,"EnterCriticalSection")),reinterpret_cast<void*>(GetProcAddress(kernel,"LeaveCriticalSection")),
        reinterpret_cast<void*>(GetProcAddress(kernel,"WaitForSingleObject")),reinterpret_cast<void*>(GetProcAddress(kernel,"Sleep")),
        ordinal(storm,255),ordinal(storm,257),ordinal(storm,258),
        reinterpret_cast<void*>(GetProcAddress(kernel,"WaitForSingleObject")),reinterpret_cast<void*>(GetProcAddress(kernel,"Sleep"))};
    const bool ready=install(slots,originals,reinterpret_cast<void*>(sb+0x16530),reinterpret_cast<void*>(sb+0x16548));
    note("native_sound_client_base",cb);note("native_sound_module_base",sb);
    note(ready?"native_sound_imports_ready":"native_sound_imports_unavailable",ready?15:0);return ready;
}
#ifdef MXL_SOUND_TEST
bool test_sound_imports(void*** slots,void** originals,void* first,void* second){return install(slots,originals,first,second);}
#endif
}
