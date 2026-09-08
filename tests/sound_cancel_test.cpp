#include "sound_cancel.h"
#include "sound_cancel_guards.h"
#include "diagnostics.h"
#include <atomic>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace mxl::sound_cancel;
namespace {
constexpr uintptr_t FreeCaller=0x12345000,CancelCaller=0x23456000;
constexpr DWORD Incoming=0x12345678,Outgoing=0x76543210,NativeException=0xe0421234;
void* const Job=reinterpret_cast<void*>(0x34567000);
void* const Buffer=reinterpret_cast<void*>(0x45678000);
std::atomic<unsigned> cancels{0},wakes{0},frees{0};
bool nested=false,other_thread=false,throw_free=false,throw_cancel=false,seh=false,waiting=false,wrong_cancel=false;
HANDLE wake_event=nullptr,complete_event=nullptr;
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void __stdcall native_cancel(void* buffer) {
    const auto error=GetLastError();require(error==Incoming,"Cancel incoming LastError changed.");
    require(buffer==Buffer || !buffer,"Cancel argument changed.");++cancels;
    if(throw_cancel)throw std::runtime_error("cancel exception");
    SetLastError(Outgoing);
}
void __stdcall native_prioritize(void* buffer,int priority) {
    require(buffer==Buffer && priority==1,"Wake arguments changed.");++wakes;
    if(waiting)SetEvent(wake_event);
    SetLastError(0x55555555);
}
void __fastcall native_free(void* job) {
    const auto incoming=GetLastError();require(incoming==Incoming && job==Job,"Release argument or LastError changed.");++frees;
    if(throw_free)throw std::runtime_error("free exception");
    if(seh)RaiseException(NativeException,0,0,nullptr);
    if(nested){nested=false;test_release(Job,FreeCaller+1);SetLastError(Incoming);}
    if(other_thread) {
        const auto before=wakes.load();std::thread peer([](){SetLastError(Incoming);test_cancel(Buffer,CancelCaller);});peer.join();
        require(wakes==before,"Scope leaked across threads.");SetLastError(Incoming);
    }
    test_cancel(Buffer,wrong_cancel?CancelCaller+1:CancelCaller);
    if(waiting)require(WaitForSingleObject(complete_event,2000)==WAIT_OBJECT_0,"Release returned before native completion.");
    SetLastError(Outgoing);
}
bool catch_seh() {
    __try {test_release(Job,FreeCaller);}
    __except(GetExceptionCode()==NativeException?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
    return false;
}
void expect_release(uintptr_t caller,unsigned added) {
    const auto before=wakes.load();SetLastError(Incoming);test_release(Job,caller);
    const auto error=GetLastError();require(error==Outgoing,"Release outgoing LastError changed.");require(wakes==before+added,"Wrong wake eligibility/count.");
}
template<size_t N,size_t R> void copy_guard(uintptr_t module,uintptr_t rva,uintptr_t preferred,
    const std::array<uint8_t,N>& bytes,const std::array<size_t,R>& relocs) {
    auto* target=reinterpret_cast<uint8_t*>(module+rva);memcpy(target,bytes.data(),N);
    for(auto offset:relocs){uint32_t value;memcpy(&value,target+offset,4);value+=uint32_t(module-preferred);memcpy(target+offset,&value,4);}
}
void guards() {
    using namespace guard_data;
    auto client=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x100000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    auto fog=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x40000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    auto storm=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x40000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    require(client && fog && storm,"Allocate guard fixtures.");
#define COPY(module,name) copy_guard(module,name##Rva,name##ImageBase,name##Bytes,name##Relocs)
    COPY(client,ClientFreeTail);COPY(fog,FogFreeWait);COPY(fog,FogCancelThunk);
    COPY(storm,StormCancel);COPY(storm,StormPrioritize);COPY(storm,StormPriorityList);
#undef COPY
    require(test_code_ready(client,fog,storm),"Relocated native guard rejected.");
    unsigned mutations=0;
    auto mutate=[&](uintptr_t address,size_t length){for(size_t i=0;i<length;++i){auto* byte=reinterpret_cast<uint8_t*>(address+i);*byte^=1;require(!test_code_ready(client,fog,storm),"Altered native code accepted.");*byte^=1;++mutations;}};
#define MUTATE(module,name) mutate(module+name##Rva,name##Bytes.size())
    MUTATE(client,ClientFreeTail);MUTATE(fog,FogFreeWait);MUTATE(fog,FogCancelThunk);
    MUTATE(storm,StormCancel);MUTATE(storm,StormPrioritize);MUTATE(storm,StormPriorityList);
#undef MUTATE
    require(!test_code_ready(1,fog,storm),"Unreadable native code accepted.");
    VirtualFree(reinterpret_cast<void*>(client),0,MEM_RELEASE);VirtualFree(reinterpret_cast<void*>(fog),0,MEM_RELEASE);VirtualFree(reinterpret_cast<void*>(storm),0,MEM_RELEASE);
    std::cout<<"Guard mutations rejected: "<<mutations<<"\n";
}
}
int main() {
    try {
        guards();require(!mxl::diag::enabled(),"Test must exercise fix without diagnostic recording.");
        auto* page=static_cast<void**>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));require(page,"Allocate imports.");
        Api api{native_free,native_cancel,native_prioritize};page[0]=reinterpret_cast<void*>(native_free);page[1]=nullptr;
        require(!test_install(page,page+1,api,FreeCaller,CancelCaller,true),"Bad import accepted.");require(page[0]==reinterpret_cast<void*>(native_free),"Validation changed free import.");
        page[1]=reinterpret_cast<void*>(native_cancel);require(!test_install(reinterpret_cast<void**>(1),page+1,api,FreeCaller,CancelCaller,true),"Unreadable import accepted.");
        DWORD old;require(VirtualProtect(page,4096,PAGE_READONLY,&old),"Protect imports.");
        require(test_install(page,page+1,api,FreeCaller,CancelCaller,true),"Install imports.");
        MEMORY_BASIC_INFORMATION info{};VirtualQuery(page,&info,sizeof(info));require(info.Protect==PAGE_READONLY,"Import protection changed.");
        // Real x86 wrappers forward unknown callers without acquiring a scope.
        uintptr_t before,after;__asm mov before,esp
        SetLastError(Incoming);reinterpret_cast<FreeFn>(page[0])(Job);require(GetLastError()==Outgoing,"Free wrapper error changed.");
        SetLastError(Incoming);reinterpret_cast<CancelFn>(page[1])(Buffer);require(GetLastError()==Outgoing,"Cancel wrapper error changed.");
        __asm mov after,esp
        require(before==after && wakes==0,"Wrapper ABI or caller guard failed.");
        expect_release(FreeCaller,1);expect_release(FreeCaller+1,0);
        wrong_cancel=true;expect_release(FreeCaller,0);wrong_cancel=false;
        test_enabled(false);expect_release(FreeCaller,0);test_enabled(true);
        nested=true;expect_release(FreeCaller,1);
        other_thread=true;expect_release(FreeCaller,1);other_thread=false;
        for(auto* flag:{&throw_free,&throw_cancel}) {
            *flag=true;bool caught=false;try{expect_release(FreeCaller,0);}catch(const std::runtime_error& e){caught=std::string(e.what()).find("exception")!=std::string::npos;}
            *flag=false;require(caught,"Native C++ exception changed.");
            const auto count=wakes.load();SetLastError(Incoming);test_cancel(Buffer,CancelCaller);require(wakes==count,"Exception left an active sound scope.");
        }
        seh=true;SetLastError(Incoming);require(catch_seh(),"Native structured exception changed.");seh=false;
        auto count=wakes.load();SetLastError(Incoming);test_cancel(Buffer,CancelCaller);require(wakes==count,"SEH left an active scope.");
        SetLastError(Incoming);test_cancel(nullptr,CancelCaller);require(wakes==count,"Null buffer was woken.");
        wake_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);complete_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(wake_event && complete_event,"Create completion events.");
        waiting=true;std::thread worker([](){if(WaitForSingleObject(wake_event,2000)==WAIT_OBJECT_0)SetEvent(complete_event);});
        expect_release(FreeCaller,1);worker.join();waiting=false;CloseHandle(wake_event);CloseHandle(complete_event);
        VirtualFree(page,0,MEM_RELEASE);
        std::cout<<"PASS: native completion retained, exact callers, nested/TLS scopes, C++/SEH unwinding, x86 ABI, LastError, guards and fix without logging.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
