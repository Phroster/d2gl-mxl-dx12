#include "audio_diagnostics.h"
#include "diagnostics.h"
#include <mmsystem.h>
#include <dsound.h>
#include <tlhelp32.h>
#include <detours/detours.h>
#include <array>
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>

namespace mxl::diag {
namespace {
// Hook real implementations, preserving COM pointers, identity, reference counts
// and every argument/result. No proxy DLL, custom mixer or sound suppression.
constexpr size_t Variants=4;
template<class Fn> struct Hooks {std::array<Fn,Variants> original{};std::array<void*,Variants> address{};bool warned=false;};
struct Change {PVOID* original;PVOID replacement;PVOID* address;};
std::mutex install_mutex;
std::atomic<unsigned> installed{0};
std::atomic<bool> frozen{false};
std::atomic_flag unknown_device=ATOMIC_FLAG_INIT,unknown_buffer=ATOMIC_FLAG_INIT,unknown_3d=ATOMIC_FLAG_INIT;
thread_local bool probe=false;
template<class Fn> void queue(Hooks<Fn>& hooks,void* address,std::array<Fn,Variants> replacements,std::vector<Change>& changes) {
    for(auto existing:hooks.address)if(existing==address)return;
    for(size_t i=0;i<Variants;++i)if(!hooks.address[i]) {
        hooks.address[i]=address;hooks.original[i]=reinterpret_cast<Fn>(address);
        changes.push_back({reinterpret_cast<PVOID*>(&hooks.original[i]),reinterpret_cast<PVOID>(replacements[i]),&hooks.address[i]});return;
    }
    if(!hooks.warned){hooks.warned=true;note("audio_variant_limit_partial_coverage");}
}
bool commit(std::vector<Change>& changes) {
    if(changes.empty())return true;
    // Allocate before suspending peers; never grow a CRT container while a
    // suspended thread might own one of its allocator locks.
    std::vector<HANDLE> threads;threads.reserve(512);
    LONG error=DetourTransactionBegin();
    if(error==NO_ERROR) {
        HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
        if(snapshot==INVALID_HANDLE_VALUE)error=GetLastError();
        else {
            THREADENTRY32 entry{};entry.dwSize=sizeof(entry);
            BOOL found=Thread32First(snapshot,&entry);
            if(!found)error=GetLastError();
            while(found && error==NO_ERROR) {
                if(entry.th32OwnerProcessID==GetCurrentProcessId() && entry.th32ThreadID!=GetCurrentThreadId()) {
                    if(threads.size()==threads.capacity()){error=ERROR_TOO_MANY_TCBS;break;}
                    HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
                    if(thread){threads.push_back(thread);error=DetourUpdateThread(thread);}
                    else if(GetLastError()!=ERROR_INVALID_PARAMETER)error=GetLastError();
                }
                found=Thread32Next(snapshot,&entry);
            }
            CloseHandle(snapshot);
        }
        for(auto& change:changes)if(error==NO_ERROR)error=DetourAttach(change.original,change.replacement);
        if(error==NO_ERROR)error=DetourTransactionCommit();else DetourTransactionAbort();
    }
    for(auto thread:threads)CloseHandle(thread);
    if(error!=NO_ERROR) {
        for(auto& change:changes){*change.original=nullptr;*change.address=nullptr;}
        note("audio_hook_install_failed",error);return false;
    }
    installed.fetch_add(unsigned(changes.size()));note("audio_hooks_installed",installed.load());return true;
}
uint64_t begin_call() {return !probe && audio_enabled()?ticks():0;}
void finish_call(Audio op,uint64_t start,HRESULT result) {if(start)audio_call(op,start,ticks(),result);}
void install_device(void* object);
void install_buffer(void* object);
void install_3d(void* object,bool listener);

#define AUDIO_SIMPLE(name,operation,signature,args) \
    using name##Fn=HRESULT(WINAPI*)signature; Hooks<name##Fn> name##_hooks; \
    template<size_t I> HRESULT WINAPI name##_hook signature { \
        const auto start=begin_call();const auto result=name##_hooks.original[I] args; \
        const auto last_error=GetLastError();finish_call(Audio::operation,start,result);SetLastError(last_error);return result; }
#define BIND(name,address) queue(name##_hooks,address,std::array<name##Fn,Variants>{name##_hook<0>,name##_hook<1>,name##_hook<2>,name##_hook<3>},changes)

AUDIO_SIMPLE(play,Play,(void* self,DWORD reserved,DWORD priority,DWORD flags),(self,reserved,priority,flags))
AUDIO_SIMPLE(stop_buffer,Stop,(void* self),(self))
AUDIO_SIMPLE(lock_buffer,Lock,(void* self,DWORD offset,DWORD bytes,void** first,DWORD* first_bytes,void** second,DWORD* second_bytes,DWORD flags),(self,offset,bytes,first,first_bytes,second,second_bytes,flags))
AUDIO_SIMPLE(unlock_buffer,Unlock,(void* self,void* first,DWORD first_bytes,void* second,DWORD second_bytes),(self,first,first_bytes,second,second_bytes))
AUDIO_SIMPLE(volume,Volume,(void* self,LONG value),(self,value))
AUDIO_SIMPLE(pan,Pan,(void* self,LONG value),(self,value))
AUDIO_SIMPLE(frequency,Frequency,(void* self,DWORD value),(self,value))
AUDIO_SIMPLE(cursor,Cursor,(void* self,DWORD value),(self,value))
AUDIO_SIMPLE(restore,Restore,(void* self),(self))
AUDIO_SIMPLE(parameters3d,Parameters3D,(void* self,LPCDS3DBUFFER parameters,DWORD apply),(self,parameters,apply))
AUDIO_SIMPLE(position3d,Position3D,(void* self,D3DVALUE x,D3DVALUE y,D3DVALUE z,DWORD apply),(self,x,y,z,apply))
AUDIO_SIMPLE(commit3d,Commit3D,(void* self),(self))

using queryFn=HRESULT(WINAPI*)(void*,REFIID,void**);Hooks<queryFn> query_hooks;
template<size_t I> HRESULT WINAPI query_hook(void* self,REFIID iid,void** result) {
    const auto hr=query_hooks.original[I](self,iid,result);
    const auto last_error=GetLastError();
    if(SUCCEEDED(hr)&&result&&*result&&audio_enabled()) {
        if(iid==IID_IDirectSound3DBuffer)install_3d(*result,false);
        else if(iid==IID_IDirectSound3DListener)install_3d(*result,true);
        else if(iid==IID_IDirectSoundBuffer8)install_buffer(*result);
    }
    SetLastError(last_error);return hr;
}
using createFn=HRESULT(WINAPI*)(void*,LPCDSBUFFERDESC,LPDIRECTSOUNDBUFFER*,LPUNKNOWN);Hooks<createFn> create_hooks;
template<size_t I> HRESULT WINAPI create_hook(void* self,LPCDSBUFFERDESC desc,LPDIRECTSOUNDBUFFER* out,LPUNKNOWN outer) {
    const auto start=begin_call();const auto hr=create_hooks.original[I](self,desc,out,outer);const auto last_error=GetLastError();finish_call(Audio::CreateBuffer,start,hr);
    if(SUCCEEDED(hr)&&out&&*out&&audio_enabled())install_buffer(*out);SetLastError(last_error);return hr;
}
using duplicateFn=HRESULT(WINAPI*)(void*,LPDIRECTSOUNDBUFFER,LPDIRECTSOUNDBUFFER*);Hooks<duplicateFn> duplicate_hooks;
template<size_t I> HRESULT WINAPI duplicate_hook(void* self,LPDIRECTSOUNDBUFFER source,LPDIRECTSOUNDBUFFER* out) {
    const auto start=begin_call();const auto hr=duplicate_hooks.original[I](self,source,out);const auto last_error=GetLastError();finish_call(Audio::DuplicateBuffer,start,hr);
    if(SUCCEEDED(hr)&&out&&*out&&audio_enabled())install_buffer(*out);SetLastError(last_error);return hr;
}
using factoryFn=HRESULT(WINAPI*)(LPCGUID,void**,LPUNKNOWN);Hooks<factoryFn> factory_hooks;
template<size_t I> HRESULT WINAPI factory_hook(LPCGUID guid,void** out,LPUNKNOWN outer) {
    const auto start=begin_call();const auto hr=factory_hooks.original[I](guid,out,outer);const auto last_error=GetLastError();finish_call(Audio::Factory,start,hr);
    if(SUCCEEDED(hr)&&out&&*out&&audio_enabled())install_device(*out);SetLastError(last_error);return hr;
}
void install_device(void* object) {
    if(frozen.load(std::memory_order_acquire)) {
        auto address=(*reinterpret_cast<void***>(object))[3];
        if(std::find(create_hooks.address.begin(),create_hooks.address.end(),address)==create_hooks.address.end() && !unknown_device.test_and_set())note("audio_unmeasured_device_variant");
        return;
    }
    try {
    std::lock_guard guard(install_mutex);auto table=*reinterpret_cast<void***>(object);std::vector<Change> changes;
    BIND(create,table[3]);BIND(duplicate,table[5]);commit(changes);
    }catch(...){note("audio_device_hook_exception");}
}
void install_buffer(void* object) {
    if(frozen.load(std::memory_order_acquire)) {
        auto address=(*reinterpret_cast<void***>(object))[12];
        if(std::find(play_hooks.address.begin(),play_hooks.address.end(),address)==play_hooks.address.end() && !unknown_buffer.test_and_set())note("audio_unmeasured_buffer_variant");
        return;
    }
    try {
    std::lock_guard guard(install_mutex);auto table=*reinterpret_cast<void***>(object);std::vector<Change> changes;
    BIND(query,table[0]);BIND(lock_buffer,table[11]);BIND(play,table[12]);BIND(cursor,table[13]);
    BIND(volume,table[15]);BIND(pan,table[16]);BIND(frequency,table[17]);BIND(stop_buffer,table[18]);
    BIND(unlock_buffer,table[19]);BIND(restore,table[20]);commit(changes);
    }catch(...){note("audio_buffer_hook_exception");}
}
void install_3d(void* object,bool listener) {
    if(frozen.load(std::memory_order_acquire)) {
        auto address=(*reinterpret_cast<void***>(object))[listener?14:19];
        if(std::find(position3d_hooks.address.begin(),position3d_hooks.address.end(),address)==position3d_hooks.address.end() && !unknown_3d.test_and_set())note("audio_unmeasured_3d_variant");
        return;
    }
    try {
    std::lock_guard guard(install_mutex);auto table=*reinterpret_cast<void***>(object);std::vector<Change> changes;
    if(listener){BIND(position3d,table[14]);BIND(commit3d,table[17]);}
    else {BIND(parameters3d,table[12]);BIND(position3d,table[19]);}
    commit(changes);
    }catch(...){note("audio_3d_hook_exception");}
}
}
unsigned audio_hook_count(){return installed.load();}
bool start_audio() {
    static bool attempted=false;if(attempted)return installed.load()!=0;attempted=true;
    struct Freeze {~Freeze(){frozen.store(true,std::memory_order_release);}} freeze;
    if(!audio_enabled())return false;
    HMODULE module=LoadLibraryExW(L"dsound.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module){note("audio_dsound_unavailable",GetLastError());return false;}
    auto factory=reinterpret_cast<factoryFn>(GetProcAddress(module,"DirectSoundCreate"));
    auto factory8=reinterpret_cast<factoryFn>(GetProcAddress(module,"DirectSoundCreate8"));
    {
        std::lock_guard guard(install_mutex);std::vector<Change> changes;
        if(factory){BIND(factory,reinterpret_cast<void*>(factory));}
        if(factory8){BIND(factory,reinterpret_cast<void*>(factory8));}
        if(!commit(changes))return false;
    }
    // Obtain method addresses even if the game's DirectSound object predates the
    // renderer. These temporary buffers are NEVER played, and never set the
    // primary format/cooperative level. Probe calls are excluded from statistics.
    probe=true;IDirectSound* device=nullptr;HRESULT hr=E_FAIL;
    if(factory)hr=factory(nullptr,reinterpret_cast<void**>(&device),nullptr);
    if(SUCCEEDED(hr)&&device) {
        install_device(device);
        WAVEFORMATEX wave{};wave.wFormatTag=WAVE_FORMAT_PCM;wave.nChannels=1;wave.nSamplesPerSec=22050;
        wave.wBitsPerSample=16;wave.nBlockAlign=2;wave.nAvgBytesPerSec=44100;
        for(bool three_d:{false,true}) {
            DSBUFFERDESC desc{};desc.dwSize=sizeof(desc);desc.dwBufferBytes=256;desc.lpwfxFormat=&wave;
            desc.dwFlags=DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLFREQUENCY|DSBCAPS_LOCSOFTWARE|(three_d?DSBCAPS_CTRL3D:DSBCAPS_CTRLPAN);
            IDirectSoundBuffer* buffer=nullptr;auto result=device->CreateSoundBuffer(&desc,&buffer,nullptr);
            if(SUCCEEDED(result)&&buffer) {
                install_buffer(buffer);
                if(three_d){IDirectSound3DBuffer* spatial=nullptr;if(SUCCEEDED(buffer->QueryInterface(IID_IDirectSound3DBuffer,reinterpret_cast<void**>(&spatial)))){install_3d(spatial,false);spatial->Release();}}
                buffer->Release();
            } else note("audio_silent_probe_buffer_failed",result);
        }
        DSBUFFERDESC primary_desc{};primary_desc.dwSize=sizeof(primary_desc);primary_desc.dwFlags=DSBCAPS_PRIMARYBUFFER|DSBCAPS_CTRL3D;
        IDirectSoundBuffer* primary=nullptr;
        if(SUCCEEDED(device->CreateSoundBuffer(&primary_desc,&primary,nullptr))&&primary) {
            install_buffer(primary);IDirectSound3DListener* listener=nullptr;
            if(SUCCEEDED(primary->QueryInterface(IID_IDirectSound3DListener,reinterpret_cast<void**>(&listener)))){install_3d(listener,true);listener->Release();}
            primary->Release();
        } else note("audio_listener_probe_unavailable");
        device->Release();
    } else note("audio_silent_probe_device_failed",hr);
    probe=false;note("audio_silent_probe_no_play",installed.load());return installed.load()!=0;
}
}
