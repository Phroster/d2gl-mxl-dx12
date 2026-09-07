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
#include <vector>

using namespace mxl::diag;
alignas(4) std::array<uint8_t,0x240> level_data{};
alignas(4) std::array<uint8_t,0x80> room_data{};
alignas(4) std::array<uint8_t,0x490> misc_data{};
alignas(4) std::array<uint8_t,0x40> map_data{};
alignas(4) std::array<uint8_t,0x2da00> lookup_image{};
constexpr DWORD incoming_error=0x10203040, outgoing_error=0x4142;
enum class ThrowAt { None, Root, Build, Dt1, Grid };
ThrowAt throw_at=ThrowAt::None;
alignas(4) std::array<uint8_t,0x3fb000> preselection_image{};
alignas(4) std::array<uint8_t,0xc80> preselection_tables{};
alignas(4) std::array<uint8_t,44*0x9c> preselection_rows{};
alignas(4) std::array<uint8_t,0x240> nested_level_data{};
uint32_t preselection_tables_global=0;
uintptr_t preselection_return_address=0;
bool preselection_mode=false,preselection_nested=false,preselection_forward_only=false,preselection_throw=false;
uint32_t current_layer=7,selector_calls=0,cache_transitions=0,nested_entry_layer=0,forward_count=0;
void* forwarded_level=nullptr;
std::vector<std::pair<uint32_t,uint32_t>> map_draws;
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
    if(preselection_mode){
        check_error();++selector_calls;
        if(current_layer!=layer){++cache_transitions;current_layer=layer;}
        SetLastError(outgoing_error);return 0x1005;
    }
    check_error();require(layer==17,"Layer fastcall argument changed.");SetLastError(outgoing_error);return 0x1005;
}
void native_map_callback(uint32_t id,uint32_t target,uint32_t room) {
    const auto saved=current_layer;volatile RevealLayerFn select=layer_mock;
    SetLastError(incoming_error);finished(select(target),0x1005);
    map_draws.emplace_back(id*16+room,current_layer);
    SetLastError(incoming_error);finished(select(saved),0x1005);
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
    if(preselection_mode){
        check_error();++forward_count;forwarded_level=level;
        if(preselection_forward_only){SetLastError(outgoing_error);return 0x2468;}
        if(preselection_throw)throw std::runtime_error("simulated preselected generation error");
        require(level==level_data.data() || level==nested_level_data.data(),"Preselection changed the level argument.");
        if(level==nested_level_data.data()){
            nested_entry_layer=current_layer;
            native_map_callback(43,23,0);
        }else{
            if(preselection_nested){
                volatile RevealInitFn nested=init_mock;
                SetLastError(incoming_error);finished(nested(nested_level_data.data()),0x2468);
            }
            for(uint32_t room=0;room<3;++room)native_map_callback(42,17,room);
        }
        SetLastError(outgoing_error);return 0x2468;
    }
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
// A real CALL instruction exposes its post-call label before entering the hook.
// The unoptimized baseline publishes that label, which becomes the only allowed
// return address in the test configuration for all subsequent calls.
__declspec(naked) uintptr_t __stdcall invoke_preselection_site(void* level) {
    __asm {
        mov eax,offset after_init
        mov preselection_return_address,eax
        mov eax,dword ptr [esp+4]
        push eax
        call init_mock
    after_init:
        ret 4
    }
}
__declspec(noinline) uintptr_t __stdcall invoke_unrelated_site(void* level) {
    volatile RevealInitFn init=init_mock;return init(level);
}
void reset_preselection_fixture() {
    preselection_image.fill(0);preselection_tables.fill(0);preselection_rows.fill(0);
    memcpy(preselection_image.data()+reveal_generation_order_site.rva,reveal_generation_order_site.bytes,32);
    memcpy(preselection_image.data()+reveal_sites[12].rva,reveal_sites[12].bytes,32);
    preselection_tables_global=uint32_t(reinterpret_cast<uintptr_t>(preselection_tables.data()));
    write32(preselection_image.data(),0x3fa958,uint32_t(reinterpret_cast<uintptr_t>(&preselection_tables_global)));
    write32(preselection_tables.data(),0xc5c,44);
    write32(preselection_tables.data(),0xc60,uint32_t(reinterpret_cast<uintptr_t>(preselection_rows.data())));
    write32(level_data.data(),0x1d0,42);write32(nested_level_data.data(),0x1d0,43);
    for(const auto id:{42u,43u}){
        write32(preselection_rows.data(),id*0x9c+8,id==42?17:23);
        write32(preselection_rows.data(),id*0x9c+0xc,32);
        write32(preselection_rows.data(),id*0x9c+0x18,32);
    }
}
struct MapResult {
    uint32_t calls,transitions,final_layer,nested_layer;
    std::vector<std::pair<uint32_t,uint32_t>> draws;
};
MapResult map_cycle(bool allowed,bool nested=false,uint32_t initial_layer=7) {
    current_layer=initial_layer;selector_calls=cache_transitions=0;map_draws.clear();nested_entry_layer=0;
    preselection_nested=nested;preselection_forward_only=false;preselection_throw=false;
    const auto saved_player_layer=current_layer;
    volatile uint32_t canary=0x10293847;
    SetLastError(incoming_error);
    finished(allowed?invoke_preselection_site(level_data.data()):invoke_unrelated_site(level_data.data()),0x2468);
    require(canary==0x10293847,"Preselection caller stack changed.");
    // Mirror Sigma's existing next selection, room draws, and root restoration.
    volatile RevealLayerFn select=layer_mock;
    SetLastError(incoming_error);finished(select(17),0x1005);
    for(uint32_t room=0;room<3;++room)map_draws.emplace_back(42*16+room,current_layer);
    SetLastError(incoming_error);finished(select(saved_player_layer),0x1005);
    return {selector_calls,cache_transitions,current_layer,nested_entry_layer,map_draws};
}
void native_fallback(void* level) {
    selector_calls=0;preselection_forward_only=true;const auto before=forward_count;
    SetLastError(incoming_error);finished(invoke_preselection_site(level),0x2468);
    require(selector_calls==0 && forward_count==before+1 && forwarded_level==level,"Invalid preselection input did not forward natively.");
    preselection_forward_only=false;
}
void preselection_scenarios() {
    preselection_mode=true;reset_preselection_fixture();
    test_reveal_preselection(0,0);
    const auto baseline=map_cycle(true);
    const auto image=reinterpret_cast<uintptr_t>(preselection_image.data());
    require(preselection_return_address && baseline.calls==8 && baseline.transitions==8 && baseline.final_layer==7 && baseline.draws.size()==6,"Native automap fixture is invalid.");
    require(test_reveal_preselection(image,preselection_return_address),"Supported preselection guard rejected.");
    const auto optimized=map_cycle(true);
    require(optimized.calls==9 && optimized.transitions==2 && optimized.final_layer==baseline.final_layer && optimized.draws==baseline.draws,"Preselection changed map output or retained redundant cache transitions.");
    const auto unrelated=map_cycle(false);
    require(unrelated.calls==baseline.calls && unrelated.transitions==baseline.transitions && unrelated.draws==baseline.draws,"Unrelated InitLevel caller was changed.");
    const auto nested=map_cycle(true,true);
    require(nested.calls==11 && nested.transitions==4 && nested.nested_layer==17 && nested.draws.size()==7 && nested.draws.front()==std::make_pair(43u*16,23u) && nested.final_layer==7,"Nested generation was preselected or its map output changed.");
    const auto already_selected=map_cycle(true,false,17);
    require(already_selected.calls==9 && already_selected.transitions==0 && already_selected.draws==baseline.draws && already_selected.final_layer==17,"Already-selected layer performed cache work.");

    // Every ordering opcode/operand except the relocated IAT address is exact.
    for(size_t i=0;i<21;++i){
        auto& byte=preselection_image[reveal_generation_order_site.rva+i];byte^=1;
        const bool accepted=test_reveal_preselection(image,preselection_return_address);
        require(accepted==(reveal_generation_order_site.mask[i]==0),"Ordering signature mask accepted an unknown instruction.");byte^=1;
    }
    preselection_image[reveal_sites[12].rva]^=1;
    require(!test_reveal_preselection(image,preselection_return_address),"Unknown layer selector accepted for preselection.");
    const auto unsupported=map_cycle(true);
    require(unsupported.calls==8 && unsupported.transitions==8 && unsupported.draws==baseline.draws,"Unsupported guard did not retain native ordering.");
    preselection_image[reveal_sites[12].rva]^=1;
    require(test_reveal_preselection(image,preselection_return_address),"Restored preselection guard rejected.");

    native_fallback(nullptr);native_fallback(reinterpret_cast<void*>(1));
    write32(level_data.data(),0x1d0,0);native_fallback(level_data.data());
    write32(level_data.data(),0x1d0,44);native_fallback(level_data.data());
    write32(level_data.data(),0x1d0,42);
    write32(preselection_tables.data(),0xc5c,4097);native_fallback(level_data.data());
    write32(preselection_tables.data(),0xc5c,44);
    write32(preselection_image.data(),0x3fa958,0);native_fallback(level_data.data());
    write32(preselection_image.data(),0x3fa958,uint32_t(reinterpret_cast<uintptr_t>(&preselection_tables_global)));
    preselection_tables_global=0;native_fallback(level_data.data());
    preselection_tables_global=uint32_t(reinterpret_cast<uintptr_t>(preselection_tables.data()));
    write32(preselection_tables.data(),0xc60,0);native_fallback(level_data.data());
    write32(preselection_tables.data(),0xc60,1);native_fallback(level_data.data());
    write32(preselection_tables.data(),0xc60,0xfffffff0);native_fallback(level_data.data());
    write32(preselection_tables.data(),0xc60,uint32_t(reinterpret_cast<uintptr_t>(preselection_rows.data())));
    write32(preselection_rows.data(),42*0x9c+0xc,0);native_fallback(level_data.data());
    write32(preselection_rows.data(),42*0x9c+0xc,32);
    write32(preselection_rows.data(),42*0x9c+0x18,0);native_fallback(level_data.data());
    write32(preselection_rows.data(),42*0x9c+0x18,32);
    preselection_throw=true;bool caught=false;SetLastError(incoming_error);
    try{invoke_preselection_site(level_data.data());}catch(const std::runtime_error& error){caught=std::string(error.what())=="simulated preselected generation error";}
    require(caught,"Preselected generation exception was swallowed.");
    preselection_throw=false;test_reveal_preselection(0,0);preselection_mode=false;
}
int wmain(int argc,wchar_t** argv) {
    try {
        const bool omit_lookup=argc==3 && !wcscmp(argv[2],L"--skip-lookup");
        const bool preselection_only=argc==3 && !wcscmp(argv[2],L"--preselection-only");
        require(argc==2 || omit_lookup || preselection_only,"Unexpected test arguments.");
        if(preselection_only){
            require(!enabled(),"Preselection-only test unexpectedly started recording.");
            RevealDeepFns deep{preset_mock,build_mock,prepare_mock,reinterpret_cast<void*>(&dt1_mock),reinterpret_cast<void*>(&grid_mock),lookup_mock,layer_mock};
            require(test_reveal_probe(root_mock,level_mock,room_mock,init_mock,load_mock,unload_mock,deep,1u<<3),"Install only gameplay InitLevel hook.");
            preselection_scenarios();require(!enabled(),"Gameplay optimization enabled recording.");
            std::cout<<"PASS: InitLevel-only preselection without diagnostics, exact caller/signature guards, identical map draws, native nested/invalid forwarding, cache-transition reduction and exception propagation.\n";
            return 0;
        }
        require(start(nullptr,argv[1]),"Start test diagnostics.");
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
        preselection_scenarios();
        stop(true);
        // Installed detours must become transparent when diagnostics stop.
        SetLastError(incoming_error);finished(root(),0x98765432);
        preselection_scenarios();
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
