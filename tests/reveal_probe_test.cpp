#include "reveal_probe.h"
#include "diagnostics.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace mxl::diag;
alignas(4) std::array<uint8_t,0x240> level_data{};
alignas(4) std::array<uint8_t,0x80> room_data{};
bool should_throw=false;
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void write32(uint8_t* data,size_t offset,uint32_t value){memcpy(data+offset,&value,4);}
__declspec(noinline) uintptr_t __stdcall load_mock(void* act,uint32_t level,uint32_t x,uint32_t y,void* room) {
    require(act==level_data.data()&&level==42&&x==12&&y==34&&room==room_data.data(),"Load arguments changed.");Sleep(1);return 0x1234;
}
__declspec(noinline) uintptr_t __stdcall unload_mock(void* act,uint32_t level,uint32_t x,uint32_t y,void* room) {
    require(act==level_data.data()&&level==42&&x==12&&y==34&&room==room_data.data(),"Unload arguments changed.");Sleep(1);return 0x5678;
}
__declspec(noinline) uintptr_t __stdcall init_mock(void* level){require(level==level_data.data(),"Init argument changed.");Sleep(2);return 0x2468;}
__declspec(noinline) uintptr_t __fastcall room_mock(void* room) {
    require(room==room_data.data(),"Room argument changed.");
    volatile RevealRoomDataFn load=load_mock,unload=unload_mock;
    require(load(level_data.data(),42,12,34,room)==0x1234,"Load return changed.");
    require(unload(level_data.data(),42,12,34,room)==0x5678,"Unload return changed.");return 0x1357;
}
__declspec(noinline) uintptr_t __fastcall level_mock(void* level) {
    require(level==level_data.data(),"Level argument changed.");volatile RevealNodeFn room=room_mock;
    require(room(room_data.data())==0x1357 && room(room_data.data())==0x1357,"Room return changed.");return 0x3579;
}
__declspec(noinline) uintptr_t __cdecl root_mock() {
    if(should_throw)throw std::runtime_error("simulated reveal error");
    volatile RevealInitFn init=init_mock;volatile RevealNodeFn level=level_mock;
    require(init(level_data.data())==0x2468 && level(level_data.data())==0x3579,"Nested return changed.");
    SetLastError(0x4142);return 0x98765432;
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2 && start(nullptr,argv[1]),"Start test diagnostics.");
        write32(level_data.data(),0x1d0,42);write32(room_data.data(),0x58,uint32_t(reinterpret_cast<uintptr_t>(level_data.data())));
        write32(room_data.data(),0x34,12);write32(room_data.data(),0x38,34);
        require(test_reveal_probe(root_mock,level_mock,room_mock,init_mock,load_mock,unload_mock),"Install guarded test hooks.");
        volatile uint32_t canary=0xA5A55A5A;volatile RevealRootFn root=root_mock;
        require(root()==0x98765432 && GetLastError()==0x4142 && canary==0xA5A55A5A,"Root return, last-error or stack changed.");
        should_throw=true;bool caught=false;try{root();}catch(const std::runtime_error&){caught=true;}require(caught,"Original exception was not preserved.");
        volatile RevealNodeFn room=room_mock;require(room(room_data.data())==0x1357,"Outside-root forwarding failed.");
        stop(true);
        std::ifstream file(std::filesystem::path(argv[1])/"reveal.csv");
        std::string line;size_t rows=0;bool root_found=false,coordinates=false;
        std::getline(file,line);while(std::getline(file,line)){
            ++rows;root_found|=line.find(",act,")!=std::string::npos;coordinates|=line.find(",-1,42,12,34,0")!=std::string::npos;
        }
        require(rows==9 && root_found && coordinates,"Wrong phase count, identity or exception cleanup.");
        std::cout<<"PASS: x86 cdecl/fastcall/stdcall arguments, return registers, last-error, stack, nested phase recording and exception cleanup.\n";
        return 0;
    }catch(const std::exception& error){stop(true);std::cerr<<error.what()<<"\n";return 1;}
}
