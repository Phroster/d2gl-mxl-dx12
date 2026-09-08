#include "asset_probe.h"
#include "diagnostics.h"
#include <wincrypt.h>
#include <array>
#include <cstdio>
#include <cstring>

namespace mxl::diag {
namespace {
using OpenFn=uint32_t(__fastcall*)(const char*,void**);
using ReadFn=uint32_t(__fastcall*)(void*,void*,uint32_t,uint32_t*,uint32_t,uint32_t,uint32_t);
using CloseFn=uint32_t(__fastcall*)(void*);
OpenFn original_open=nullptr;
ReadFn original_read=nullptr;
CloseFn original_close=nullptr;
bool installed=false;

unsigned copy_name(const char* name,char (&out)[96]) noexcept {
    __try {
        if(!name)return 2;
        for(size_t i=0;i<95;++i){out[i]=name[i];if(!out[i])return 0;}
        out[95]=0;return name[95]?1:0;
    } __except(EXCEPTION_EXECUTE_HANDLER) {out[0]=0;return 2;}
}
uint32_t output32(const void* address,bool& valid) noexcept {
    __try {if(address){const auto value=*static_cast<const uint32_t*>(address);valid=true;return value;}}
    __except(EXCEPTION_EXECUTE_HANDLER) {}
    valid=false;return 0;
}
template<unsigned Source> uint32_t __fastcall open_hook(const char* name,void** handle) {
    if(!enabled())return original_open(name,handle);
    const auto incoming=GetLastError();char snapshot[96]{};const auto status=copy_name(name,snapshot);
    const auto began=ticks();SetLastError(incoming);
    const auto result=original_open(name,handle);const auto error=GetLastError();const auto ended=ticks();
    bool valid=false;const auto id=result?output32(handle,valid):0;
    asset_call(AssetOperation::Open,Source,valid?id:0,began,ended,snapshot,status,0,0,valid,result);
    SetLastError(error);return result;
}
template<unsigned Source> uint32_t __fastcall read_hook(void* handle,void* buffer,uint32_t requested,
    uint32_t* completed,uint32_t fifth,uint32_t sixth,uint32_t seventh) {
    if(!enabled())return original_read(handle,buffer,requested,completed,fifth,sixth,seventh);
    const auto incoming=GetLastError();const auto began=ticks();SetLastError(incoming);
    const auto result=original_read(handle,buffer,requested,completed,fifth,sixth,seventh);
    const auto error=GetLastError();const auto ended=ticks();
    bool valid=false;const auto amount=result?output32(completed,valid):0;
    asset_call(AssetOperation::Read,Source,reinterpret_cast<uintptr_t>(handle),began,ended,"",3,requested,amount,valid,result);
    SetLastError(error);return result;
}
template<unsigned Source> uint32_t __fastcall close_hook(void* handle) {
    if(!enabled())return original_close(handle);
    const auto incoming=GetLastError();const auto began=ticks();SetLastError(incoming);
    const auto result=original_close(handle);const auto error=GetLastError();const auto ended=ticks();
    asset_call(AssetOperation::Close,Source,reinterpret_cast<uintptr_t>(handle),began,ended,"",3,0,0,false,result);
    SetLastError(error);return result;
}
bool file_hash(HMODULE module,const char* expected) {
    wchar_t path[32768]{};if(!GetModuleFileNameW(module,path,32768))return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;bool good=false;
    if(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) && CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
        std::array<BYTE,32768> buffer{};DWORD count=0;bool complete=true;
        for(;;){if(!ReadFile(file,buffer.data(),DWORD(buffer.size()),&count,nullptr)){complete=false;break;}if(!count)break;if(!CryptHashData(hash,buffer.data(),count,0)){complete=false;break;}}
        BYTE digest[32]{};DWORD size=sizeof(digest);
        if(complete && CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)){
            char text[65]{};for(size_t i=0;i<32;++i)sprintf_s(text+i*2,3,"%02x",digest[i]);good=!strcmp(text,expected);
        }
    }
    if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);return good;
}
void* read_slot(void** slot) noexcept {
    __try {return slot?*slot:nullptr;}__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
bool exchange(void** slot,void* expected,void* replacement) noexcept {
    if(!slot || reinterpret_cast<uintptr_t>(slot)%alignof(void*))return false;
    DWORD protection=0;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection))return false;
    const auto previous=InterlockedCompareExchangePointer(slot,replacement,expected);
    DWORD ignored=0;const bool restored=VirtualProtect(slot,sizeof(void*),protection,&ignored)!=0;
    if(!restored)note("asset_import_protection_restore_failed",GetLastError());
    return previous==expected;
}
bool install(void*** slots,OpenFn open,ReadFn read,CloseFn close) {
    if(installed)return true;
    if(!slots || !open || !read || !close)return false;
    void* expected[]={reinterpret_cast<void*>(open),reinterpret_cast<void*>(read),reinterpret_cast<void*>(close)};
    void* replacements[]={reinterpret_cast<void*>(&open_hook<0>),reinterpret_cast<void*>(&read_hook<0>),reinterpret_cast<void*>(&close_hook<0>),
        reinterpret_cast<void*>(&open_hook<1>),reinterpret_cast<void*>(&read_hook<1>),reinterpret_cast<void*>(&close_hook<1>)};
    // Validate every existing import before any write. Atomic pointer exchanges
    // leave calls already in flight untouched and refuse another patcher's slot.
    for(size_t i=0;i<6;++i){
        if(read_slot(slots[i])!=expected[i%3])return false;
        for(size_t j=0;j<i;++j)if(slots[j]==slots[i])return false;
    }
    original_open=open;original_read=read;original_close=close;
    size_t changed=0;
    for(;changed<6;++changed)if(!exchange(slots[changed],expected[changed%3],replacements[changed]))break;
    if(changed!=6){
        while(changed){--changed;if(!exchange(slots[changed],replacements[changed],expected[changed%3]))note("asset_import_rollback_failed",changed);}
        return false;
    }
    installed=true;return true;
}
}
bool start_assets() {
    if(!assets_enabled())return false;
    if(installed)return true;
    const auto fog=GetModuleHandleW(L"Fog.dll"),cmp=GetModuleHandleW(L"D2CMP.dll"),sound=GetModuleHandleW(L"D2sound.dll");
    if(!fog || !cmp || !sound ||
        !file_hash(fog,"53f015869c495c760d2c5a6d8d836c8b5f0f5437b69ca0dbf0d977dfa5ec96cf") ||
        !file_hash(cmp,"2ee205f484161c5ed854f30aed12565a7ac3e97fd1a838e697aaefe4dc349756") ||
        !file_hash(sound,"b24dfb0159c76867e7f3d14602e73bccc37d59b7c8d2def88213b01084b61a72")){
        note("asset_import_identity_unavailable");return false;
    }
    // Confirmed installed imports: Fog #10102 (open), #10104 (read),
    // #10103 (close). Fastcall read has two register and five stack arguments.
    const auto cb=reinterpret_cast<uintptr_t>(cmp),sb=reinterpret_cast<uintptr_t>(sound);
    void** slots[]={reinterpret_cast<void**>(cb+0x1c010),reinterpret_cast<void**>(cb+0x1c014),reinterpret_cast<void**>(cb+0x1c008),
        reinterpret_cast<void**>(sb+0xf024),reinterpret_cast<void**>(sb+0xf02c),reinterpret_cast<void**>(sb+0xf020)};
    const auto open=reinterpret_cast<OpenFn>(GetProcAddress(fog,MAKEINTRESOURCEA(10102)));
    const auto read=reinterpret_cast<ReadFn>(GetProcAddress(fog,MAKEINTRESOURCEA(10104)));
    const auto close=reinterpret_cast<CloseFn>(GetProcAddress(fog,MAKEINTRESOURCEA(10103)));
    const bool ready=install(slots,open,read,close);
    note(ready?"asset_imports_ready":"asset_imports_unavailable",ready?6:0);return ready;
}
#ifdef MXL_ASSET_TEST
bool test_asset_imports(void*** slots,AssetOpenFn open,AssetReadFn read,AssetCloseFn close) {return install(slots,open,read,close);}
#endif
}
