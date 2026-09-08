#include "sound_probe.h"
#include "diagnostics.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <vector>

using namespace mxl::diag;
namespace {
constexpr DWORD Incoming=0x12345678,Outgoing=0x76543210;
const char path[]="data\\global\\sfx\\act5\\co\"ld,first.wav";
void* const job=reinterpret_cast<void*>(0x12340000);
void* const data=reinterpret_cast<void*>(0x34560000);
std::atomic<unsigned> calls[15]{};bool throws=false;
CRITICAL_SECTION locks[2];
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void entered(unsigned slot) {
    const auto error=GetLastError();++calls[slot];require(error==Incoming,"Incoming LastError changed.");
    if(throws)throw std::runtime_error("native failure");
    SetLastError(Outgoing);
}
void* __fastcall load(void* pool,const char* name,BOOL async,LONG offset,int size,void* target,void* callback,int priority,const char* source,int line) {
    entered(0);require(pool==data && name==path && async==7 && offset==-9 && size==123 && target==job &&
        callback==data && priority==-2 && source==path && line==0x31b,"Async load register/stack arguments changed.");
    SetLastError(Outgoing);return job;
}
void* __fastcall buffer(void* id){entered(1);require(id==job,"Async buffer argument changed.");Sleep(2);SetLastError(Outgoing);return data;}
void __fastcall free_job(void* id){entered(2);require(id==job,"Async free argument changed.");SetLastError(Outgoing);}
uint32_t __fastcall open(const char* name,void** output){entered(3);require(name==path && output,"Open arguments changed.");*output=job;SetLastError(Outgoing);return 0x1234;}
uint32_t __fastcall read(void* handle,void* target,uint32_t bytes,uint32_t* completed,uint32_t a5,uint32_t a6,uint32_t a7) {
    entered(4);require(handle==job && target==data && bytes==17 && completed && a5==0x155 && a6==0x266 && a7==0x377,"Read arguments changed.");
    *completed=17;SetLastError(Outgoing);return 0x2345;
}
uint32_t __fastcall close(void* id){entered(5);require(id==job,"Close argument changed.");SetLastError(Outgoing);return 0x3456;}
void WINAPI enter(LPCRITICAL_SECTION lock){entered(6);EnterCriticalSection(lock);SetLastError(Outgoing);}
void WINAPI leave(LPCRITICAL_SECTION lock){entered(7);LeaveCriticalSection(lock);SetLastError(Outgoing);}
DWORD WINAPI wait(HANDLE handle,DWORD timeout){entered(8);require(handle==job && timeout==123,"Wait arguments changed.");SetLastError(Outgoing);return WAIT_TIMEOUT;}
void WINAPI sleep(DWORD timeout){entered(9);require(timeout==456,"Sleep argument changed.");SetLastError(Outgoing);}
int WINAPI music_begin(HANDLE file,int a2,unsigned a3,DWORD offset,int a5,int a6,int a7) {
    entered(10);require(file==job && a2==-11 && a3==12 && offset==13 && a5==-14 && a6==15 && a7==-16,"Music begin arguments changed.");SetLastError(Outgoing);return -7;
}
BOOL WINAPI music_end(HANDLE file){entered(11);require(file==job,"Music end argument changed.");SetLastError(Outgoing);return 4;}
BOOL WINAPI music_position(HANDLE file,void* position,void* length){entered(12);require(file==job && position==data && length==job,"Music position arguments changed.");SetLastError(Outgoing);return 5;}
void outgoing(){const auto error=GetLastError();require(error==Outgoing,"Outgoing LastError changed.");}
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> parts;std::stringstream s(line);std::string value;while(std::getline(s,value,','))parts.push_back(value);return parts;
}
void call(void** page,unsigned op) {
            uintptr_t before=0,after=0;uint32_t completed=0;void* output=nullptr;
            __asm mov before,esp
            SetLastError(Incoming);
            switch(op) {
                case 0:require(reinterpret_cast<decltype(&load)>(page[op])(data,path,7,-9,123,job,data,-2,path,0x31b)==job,"Load return changed.");break;
                case 1:require(reinterpret_cast<decltype(&buffer)>(page[op])(job)==data,"Buffer return changed.");break;
                case 2:reinterpret_cast<decltype(&free_job)>(page[op])(job);break;
                case 3:require(reinterpret_cast<decltype(&open)>(page[op])(path,&output)==0x1234 && output==job,"Open output changed.");break;
                case 4:require(reinterpret_cast<decltype(&read)>(page[op])(job,data,17,&completed,0x155,0x266,0x377)==0x2345 && completed==17,"Read output changed.");break;
                case 5:require(reinterpret_cast<decltype(&close)>(page[op])(job)==0x3456,"Close return changed.");break;
                case 6:reinterpret_cast<decltype(&enter)>(page[op])(&locks[0]);break;
                case 7:reinterpret_cast<decltype(&leave)>(page[op])(&locks[0]);break;
                case 8:case 13:require(reinterpret_cast<decltype(&wait)>(page[op])(job,123)==WAIT_TIMEOUT,"Wait return changed.");break;
                case 9:case 14:reinterpret_cast<decltype(&sleep)>(page[op])(456);break;
                case 10:require(reinterpret_cast<decltype(&music_begin)>(page[op])(job,-11,12,13,-14,15,-16)==-7,"Music begin return changed.");break;
                case 11:require(reinterpret_cast<decltype(&music_end)>(page[op])(job)==4,"Music end return changed.");break;
                case 12:require(reinterpret_cast<decltype(&music_position)>(page[op])(job,data,job)==5,"Music position return changed.");break;
            }
            outgoing();__asm mov after,esp
            require(before==after,"Native calling convention corrupted stack.");
}
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"Expected output directory.");
        void* originals[]={reinterpret_cast<void*>(&load),reinterpret_cast<void*>(&buffer),reinterpret_cast<void*>(&free_job),
            reinterpret_cast<void*>(&open),reinterpret_cast<void*>(&read),reinterpret_cast<void*>(&close),
            reinterpret_cast<void*>(&enter),reinterpret_cast<void*>(&leave),reinterpret_cast<void*>(&wait),reinterpret_cast<void*>(&sleep),
            reinterpret_cast<void*>(&music_begin),reinterpret_cast<void*>(&music_end),reinterpret_cast<void*>(&music_position),
            reinterpret_cast<void*>(&wait),reinterpret_cast<void*>(&sleep)};
        auto* page=static_cast<void**>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));require(page,"Allocate test imports.");
        void** slots[15]{};for(unsigned i=0;i<15;++i){page[i]=originals[i];slots[i]=page+i;}
        InitializeCriticalSection(&locks[0]);InitializeCriticalSection(&locks[1]);
        page[14]=nullptr;require(!test_sound_imports(slots,originals,&locks[0],&locks[1]),"Mixed imports accepted.");
        for(unsigned i=0;i<14;++i)require(page[i]==originals[i],"Failed validation mutated an import.");
        page[14]=originals[14];slots[14]=slots[9];require(!test_sound_imports(slots,originals,&locks[0],&locks[1]),"Duplicate imports accepted.");
        slots[14]=reinterpret_cast<void**>(1);require(!test_sound_imports(slots,originals,&locks[0],&locks[1]),"Invalid import accepted.");slots[14]=page+14;
        DWORD old=0;require(VirtualProtect(page,4096,PAGE_READONLY,&old),"Protect test imports.");
        require(start(nullptr,argv[1]),"Start logger.");
        require(test_sound_imports(slots,originals,&locks[0],&locks[1]),"Install test imports.");
        MEMORY_BASIC_INFORMATION info{};VirtualQuery(page,&info,sizeof(info));require(info.Protect==PAGE_READONLY,"Import protection changed.");
        for(unsigned i=0;i<15;++i)call(page,i);
        throws=true;
        for(unsigned i=0;i<15;++i){bool caught=false;try{call(page,i);}catch(const std::runtime_error& e){caught=std::string(e.what())=="native failure";}require(caught,"Native exception changed.");}
        throws=false;
        // Recursion must emit one outer hold, and a contended acquisition must
        // retain actual synchronization while reporting both owner and waiter.
        call(page,6);call(page,6);Sleep(8);call(page,7);call(page,7);
        HANDLE acquired=CreateEventW(nullptr,TRUE,FALSE,nullptr);require(acquired,"Create synchronization event.");
        std::thread owner([&](){SetLastError(Incoming);reinterpret_cast<decltype(&enter)>(page[6])(&locks[1]);SetEvent(acquired);Sleep(25);SetLastError(Incoming);reinterpret_cast<decltype(&leave)>(page[7])(&locks[1]);});
        WaitForSingleObject(acquired,INFINITE);
        SetLastError(Incoming);reinterpret_cast<decltype(&enter)>(page[6])(&locks[1]);outgoing();
        SetLastError(Incoming);reinterpret_cast<decltype(&leave)>(page[7])(&locks[1]);outgoing();
        owner.join();CloseHandle(acquired);
        // Leave recording disabled while calls and lock recursion still work.
        stop(true);for(unsigned i=0;i<15;++i)call(page,i);
        std::ifstream file(std::filesystem::path(argv[1])/"native-sound.csv");std::string line,all;unsigned recursive_holds=0;bool wait_seen=false;
        while(std::getline(file,line)) {
            all+=line+'\n';auto fields=split(line);
            if(fields.size()<6)continue;
            if(fields[0]=="sound_lock_hold" && std::stoull(fields[5])==reinterpret_cast<uintptr_t>(&locks[0]))++recursive_holds;
            if(fields[0]=="sound_lock_wait" && std::stod(fields[3])>=10)wait_seen=true;
        }
        require(recursive_holds==1,"Recursive critical section hold double counted or lost.");
        require(wait_seen,"Contended native acquisition missing.");
        require(all.find("co\"\"ld,first.wav")!=std::string::npos,"Native path CSV escaping broken.");
        require(all.find("async_buffer,")!=std::string::npos && all.find("async_free,")!=std::string::npos,"Async detail records missing.");
        DeleteCriticalSection(&locks[0]);DeleteCriticalSection(&locks[1]);VirtualFree(page,0,MEM_RELEASE);
        std::cout<<"PASS: 15 native imports, x86 ABI, LastError, outputs, exceptions, recursive/contended locks, CSV paths and disabled forwarding.\n";return 0;
    }catch(const std::exception& error){stop(true);std::cerr<<error.what()<<"\n";return 1;}
}
