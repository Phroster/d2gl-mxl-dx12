#include "reveal_probe.h"
#include "reveal_signatures.h"
#include "diagnostics.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace mxl::diag;
alignas(4) std::array<uint8_t,0x240> level_data{};
alignas(4) std::array<uint8_t,0x80> room_data{};
alignas(4) std::array<uint8_t,0x490> misc_data{};
alignas(4) std::array<uint8_t,0x40> map_data{};
alignas(4) std::array<uint8_t,0x2da00> lookup_image{};
constexpr DWORD incoming_error=0x10203040, outgoing_error=0x4142;
enum class ThrowAt { None, Root, Build, Dt1, Grid };
ThrowAt throw_at=ThrowAt::None;
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void write32(uint8_t* data,size_t offset,uint32_t value){memcpy(data+offset,&value,4);}
void check_error(){require(GetLastError()==incoming_error,"Incoming last-error changed.");}
void finished(uintptr_t value,uintptr_t expected){require(value==expected && GetLastError()==outgoing_error,"Return or outgoing last-error changed.");}
__declspec(noinline) uintptr_t __stdcall load_mock(void* act,uint32_t level,uint32_t x,uint32_t y,void* room) {
    check_error();require(act==level_data.data()&&level==42&&x==12&&y==34&&room==room_data.data(),"Load arguments changed.");Sleep(1);SetLastError(outgoing_error);return 0x1234;
}
__declspec(noinline) uintptr_t __stdcall unload_mock(void* act,uint32_t level,uint32_t x,uint32_t y,void* room) {
    check_error();require(act==level_data.data()&&level==42&&x==12&&y==34&&room==room_data.data(),"Unload arguments changed.");Sleep(1);SetLastError(outgoing_error);return 0x5678;
}
__declspec(noinline) uintptr_t __stdcall build_mock(void* level,void* map,uint32_t flags,uint32_t single_room) {
    check_error();require(level==level_data.data() && map==map_data.data() && flags==0x2468 && single_room==1,"Build-area arguments changed.");
    if(throw_at==ThrowAt::Build)throw std::runtime_error("simulated build error");
    SetLastError(outgoing_error);return 0x1001;
}
__declspec(noinline) uintptr_t __stdcall prepare_mock(void* room) {
    check_error();require(room==room_data.data(),"Prepare-room argument changed.");SetLastError(outgoing_error);return 0x1002;
}
__declspec(noinline) uintptr_t __stdcall dt1_body(void* room) {
    check_error();require(room==room_data.data(),"DT1 register argument changed.");
    if(throw_at==ThrowAt::Dt1)throw std::runtime_error("simulated DT1 error");
    SetLastError(outgoing_error);return 0x1003;
}
__declspec(noinline) uintptr_t __stdcall grid_body(void* room,void* pool) {
    check_error();require(room==room_data.data() && pool==misc_data.data(),"Grid register arguments changed.");
    if(throw_at==ThrowAt::Grid)throw std::runtime_error("simulated grid error");
    SetLastError(outgoing_error);return 0x1004;
}
// Give Detours a complete five-byte prologue before the register conversion.
__declspec(naked) uintptr_t dt1_mock() {
    __asm {
        nop
        nop
        nop
        nop
        nop
        push esi
        call dt1_body
        ret
    }
}
__declspec(naked) uintptr_t grid_mock() {
    __asm {
        nop
        nop
        nop
        nop
        nop
        push ecx
        push eax
        call grid_body
        ret
    }
}
// Use real register callers, checking nonvolatile registers and stack balance.
__declspec(noinline) uintptr_t invoke_dt1(void* room) {
    uintptr_t result=0,before=0,after=0,seen_esi=0,seen_ebx=0,seen_edi=0,seen_ebp=0,frame=0;
    __asm {
        mov before,esp
        mov frame,ebp
        mov esi,room
        mov ebx,0x12345678
        mov edi,0x76543210
        call dt1_mock
        mov result,eax
        mov after,esp
        mov seen_esi,esi
        mov seen_ebx,ebx
        mov seen_edi,edi
        mov seen_ebp,ebp
    }
    const auto error=GetLastError();
    require(before==after && frame==seen_ebp && seen_esi==reinterpret_cast<uintptr_t>(room) && seen_ebx==0x12345678 && seen_edi==0x76543210,"DT1 stack/nonvolatile register corruption.");
    SetLastError(error);return result;
}
__declspec(noinline) uintptr_t invoke_grid(void* room,void* pool) {
    uintptr_t result=0,before=0,after=0,seen_esi=0,seen_ebx=0,seen_edi=0,seen_ebp=0,frame=0;
    __asm {
        mov before,esp
        mov frame,ebp
        mov esi,0x55667788
        mov ebx,0x12345678
        mov edi,0x76543210
        mov eax,room
        mov ecx,pool
        call grid_mock
        mov result,eax
        mov after,esp
        mov seen_esi,esi
        mov seen_ebx,ebx
        mov seen_edi,edi
        mov seen_ebp,ebp
    }
    const auto error=GetLastError();
    require(before==after && frame==seen_ebp && seen_esi==0x55667788 && seen_ebx==0x12345678 && seen_edi==0x76543210,"Grid stack/nonvolatile register corruption.");
    SetLastError(error);return result;
}
__declspec(noinline) uintptr_t __fastcall lookup_mock(void* misc,uint32_t level) {
    check_error();require(misc==misc_data.data() && level==42,"Lookup fastcall arguments changed.");SetLastError(outgoing_error);return reinterpret_cast<uintptr_t>(level_data.data());
}
__declspec(noinline) uintptr_t __fastcall layer_mock(uint32_t layer) {
    check_error();require(layer==17,"Layer fastcall argument changed.");SetLastError(outgoing_error);return 0x1005;
}
__declspec(noinline) uintptr_t __fastcall preset_mock(void* level) {
    check_error();require(level==level_data.data(),"Preset argument changed.");
    volatile RevealBuildAreaFn build=build_mock;volatile RevealInitFn prepare=prepare_mock;
    SetLastError(incoming_error);finished(build(level,map_data.data(),0x2468,1),0x1001);
    SetLastError(incoming_error);finished(prepare(room_data.data()),0x1002);
    SetLastError(incoming_error);finished(invoke_dt1(room_data.data()),0x1003);
    SetLastError(incoming_error);finished(invoke_grid(room_data.data(),misc_data.data()),0x1004);
    SetLastError(outgoing_error);return 0x1006;
}
__declspec(noinline) uintptr_t __stdcall init_mock(void* level) {
    check_error();require(level==level_data.data(),"Init argument changed.");volatile RevealNodeFn preset=preset_mock;
    SetLastError(incoming_error);finished(preset(level),0x1006);SetLastError(outgoing_error);return 0x2468;
}
__declspec(noinline) uintptr_t __fastcall room_mock(void* room) {
    check_error();require(room==room_data.data(),"Room argument changed.");volatile RevealRoomDataFn load=load_mock,unload=unload_mock;
    SetLastError(incoming_error);finished(load(level_data.data(),42,12,34,room),0x1234);
    SetLastError(incoming_error);finished(unload(level_data.data(),42,12,34,room),0x5678);SetLastError(outgoing_error);return 0x1357;
}
__declspec(noinline) uintptr_t __fastcall level_mock(void* level) {
    check_error();require(level==level_data.data(),"Level argument changed.");volatile RevealNodeFn room=room_mock;
    SetLastError(incoming_error);finished(room(room_data.data()),0x1357);
    SetLastError(incoming_error);finished(room(room_data.data()),0x1357);SetLastError(outgoing_error);return 0x3579;
}
__declspec(noinline) uintptr_t __cdecl root_mock() {
    check_error();if(throw_at==ThrowAt::Root)throw std::runtime_error("simulated root error");
    volatile RevealLookupFn lookup=lookup_mock;volatile RevealInitFn init=init_mock;volatile RevealNodeFn level=level_mock;volatile RevealLayerFn layer=layer_mock;
    SetLastError(incoming_error);finished(lookup(misc_data.data(),42),reinterpret_cast<uintptr_t>(level_data.data()));
    SetLastError(incoming_error);finished(init(level_data.data()),0x2468);
    SetLastError(incoming_error);finished(layer(17),0x1005);
    SetLastError(incoming_error);finished(level(level_data.data()),0x3579);
    SetLastError(outgoing_error);return 0x98765432;
}
int wmain(int argc,wchar_t** argv) {
    try {
        const bool omit_lookup=argc==3 && !wcscmp(argv[2],L"--skip-lookup");
        require((argc==2 || omit_lookup) && start(nullptr,argv[1]),"Start test diagnostics.");
        const uint32_t sites=((1u<<13)-1)&~(omit_lookup?(1u<<11):0u);
        const auto lookup_base=reinterpret_cast<uintptr_t>(lookup_image.data());
        memcpy(lookup_image.data()+0x2d9b0,reveal_sites[11].bytes,32);
        require(test_reveal_lookup_signature(lookup_base),"Pristine lookup signature rejected.");
        write32(lookup_image.data(),0x2d9cb,0x234);write32(lookup_image.data(),0x2d9db,0x8d);
        require(test_reveal_lookup_signature(lookup_base),"Exact Sigma lookup extension rejected.");
        write32(lookup_image.data(),0x2d9cb,0x238);
        require(!test_reveal_lookup_signature(lookup_base),"Unknown lookup size accepted.");
        write32(lookup_image.data(),0x2d9cb,0x234);write32(lookup_image.data(),0x2d9db,0x8c);
        require(!test_reveal_lookup_signature(lookup_base),"Mismatching Sigma zero-init count accepted.");
        write32(level_data.data(),0x1d0,42);write32(level_data.data(),0x1b4,uint32_t(reinterpret_cast<uintptr_t>(misc_data.data())));
        write32(room_data.data(),0x58,uint32_t(reinterpret_cast<uintptr_t>(level_data.data())));write32(room_data.data(),0x34,12);write32(room_data.data(),0x38,34);
        RevealDeepFns deep{preset_mock,build_mock,prepare_mock,reinterpret_cast<void*>(&dt1_mock),reinterpret_cast<void*>(&grid_mock),lookup_mock,layer_mock};
        auto invalid_deep=deep;invalid_deep.dt1=nullptr;
        require(!test_reveal_probe(root_mock,level_mock,room_mock,init_mock,load_mock,unload_mock,invalid_deep,sites),"Partial hook transaction accepted a missing deep entrypoint.");
        SetLastError(incoming_error);finished(root_mock(),0x98765432);
        require(test_reveal_probe(root_mock,level_mock,room_mock,init_mock,load_mock,unload_mock,deep,sites),"Install guarded test hooks.");
        volatile uint32_t canary=0xA5A55A5A;volatile RevealRootFn root=root_mock;
        SetLastError(incoming_error);finished(root(),0x98765432);require(canary==0xA5A55A5A,"Root stack changed.");
        for(const auto stage:{ThrowAt::Root,ThrowAt::Build,ThrowAt::Dt1,ThrowAt::Grid}){
            throw_at=stage;bool caught=false;SetLastError(incoming_error);
            try{root();}catch(const std::runtime_error& error){caught=std::string(error.what()).find("simulated ")==0;}
            require(caught,"Original exception was not preserved.");
            // Every deep function is called outside a root after an exception;
            // any dangling trace context would add rows or crash here.
            throw_at=ThrowAt::None;volatile RevealNodeFn preset=preset_mock;volatile RevealLookupFn lookup=lookup_mock;volatile RevealLayerFn layer=layer_mock;
            SetLastError(incoming_error);finished(preset(level_data.data()),0x1006);
            SetLastError(incoming_error);finished(lookup(misc_data.data(),42),reinterpret_cast<uintptr_t>(level_data.data()));
            SetLastError(incoming_error);finished(layer(17),0x1005);
        }
        SetLastError(incoming_error);finished(root(),0x98765432);
        stop(true);
        // Installed detours must become transparent when diagnostics stop.
        SetLastError(incoming_error);finished(root(),0x98765432);
        std::ifstream file(std::filesystem::path(argv[1])/"reveal.csv");
        std::string line;size_t rows=0;bool coordinates=false,layer_identity=false;std::map<std::string,size_t> phases;
        std::getline(file,line);while(std::getline(file,line)){
            ++rows;coordinates|=line.find(",-1,42,12,34,0")!=std::string::npos;
            layer_identity|=line.find(",automap_layer,")!=std::string::npos && line.find(",-1,-1,17,-1,0")!=std::string::npos;
            for(const auto* phase:{"act","level_rooms","room","level_generation","load_room","unload_room","preset_generation","preset_build_area","preset_room_prepare","dt1_load","room_tile_grid","level_lookup","automap_layer"})
                if(line.find(std::string(",")+phase+",")!=std::string::npos)++phases[phase];
        }
        // The production writer intentionally drops on lock contention. Require
        // every missing phase to be covered by that counter, and reject any
        // excess or unexpected rows rather than assuming recording is lossless.
        const size_t expected_rows=omit_lookup?35:40;
        const auto dropped=dropped_records();
        require(rows<=expected_rows && rows+dropped>=expected_rows && coordinates && layer_identity,"Wrong phase count, identity or exception cleanup.");
        const std::map<std::string,size_t> expected_phases={{"act",2},{"level_rooms",2},{"room",4},
            {"level_generation",2},{"load_room",4},{"unload_room",4},{"preset_generation",2},
            {"preset_build_area",4},{"preset_room_prepare",4},{"dt1_load",3},{"room_tile_grid",2},
            {"level_lookup",omit_lookup?0u:5u},{"automap_layer",2}};
        size_t missing=0;
        for(const auto& [phase,count]:expected_phases){
            require(phases[phase]<=count,"Unexpected nested deep phase count.");missing+=count-phases[phase];
        }
        require(missing<=dropped,"Missing deep phases without a recorded drop.");
        std::cout<<"PASS: "<<(omit_lookup?12:13)<<" selected hooks, x86 cdecl/fastcall/stdcall/register arguments, EAX, nonvolatile registers, incoming/outgoing last-error, stack, nested phases, deep exceptions and inactive forwarding.\n";
        return 0;
    }catch(const std::exception& error){stop(true);std::cerr<<error.what()<<"\n";return 1;}
}
