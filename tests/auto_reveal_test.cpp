#include "auto_reveal.h"
#include "diagnostics.h"
#include "audio_diagnostics.h"
#include <array>
#include <vector>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <filesystem>

namespace {
std::vector<uint8_t> sigma(0x400000);
std::array<uint8_t,0x100> state{},player{},act{},path{},room1{},room2{},layer{};
std::array<uint8_t,0x500> misc{};
std::array<uint8_t,0x240> level{};
std::array<uint8_t,0xd00> tables{};
uint32_t player_global=0,tables_global=0,layer_global=0;
HWND window=nullptr;
UINT message=0;
bool in_game=true, finish=true,nested=false,throw_original=false,inside_frame=false;
unsigned calls=0,frames_begun=0,presentations=0;
DWORD owner=0;
void require(bool good,const char* why){if(!good)throw std::runtime_error(why);}
uint32_t address(const void* p){return uint32_t(reinterpret_cast<uintptr_t>(p));}
void put(void* p,size_t offset,uint32_t value){memcpy(static_cast<uint8_t*>(p)+offset,&value,4);}
uint32_t get(const void* p,size_t offset){uint32_t v;memcpy(&v,static_cast<const uint8_t*>(p)+offset,4);return v;}
void flag(uint32_t value){put(state.data(),0x1c+get(act.data(),0x14)*4,value);}
void pump(){MSG msg;unsigned count=0;while(PeekMessageW(&msg,window,message,message,PM_REMOVE)){
    require(++count<100,"Recursive message queue.");DispatchMessageW(&msg);
}}
void frame(){
    ++frames_begun;inside_frame=true;
    mxl::reveal::begin_frame(window);mxl::reveal::end_frame(window,in_game);
    inside_frame=false;++presentations;
}
uintptr_t __cdecl reveal_mock(){
    require(GetCurrentThreadId()==owner,"Reveal ran off the game thread.");
    require(!inside_frame,"Reveal ran inside the renderer frame.");++calls;
    if(throw_original)throw std::runtime_error("original failed");
    if(nested){mxl::reveal::before_scene(window);frame();pump();mxl::reveal::before_scene(window);}
    if(finish)flag(1);
    SetLastError(0x6666);return 0x1234;
}
LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    if(mxl::reveal::window_message(hwnd,msg,w,in_game))return 0;
    return DefWindowProcW(hwnd,msg,w,l);
}
void setup(){
    player_global=address(player.data());tables_global=address(tables.data());layer_global=address(layer.data());
    put(sigma.data(),0x3fdecc,address(state.data()));put(sigma.data(),0x3fa130,address(&player_global));
    put(sigma.data(),0x3fa958,address(&tables_global));put(sigma.data(),0x3fa174,address(&layer_global));
    put(player.data(),0x1c,address(act.data()));put(player.data(),0x2c,address(path.data()));
    put(act.data(),0x14,2);put(act.data(),0x48,address(misc.data()));put(misc.data(),0x46c,address(act.data()));
    put(path.data(),0x1c,address(room1.data()));put(room1.data(),0x10,address(room2.data()));
    put(room2.data(),0x30,address(room1.data()));put(room2.data(),0x58,address(level.data()));
    put(level.data(),0x1b4,address(misc.data()));put(level.data(),0x10,address(room2.data()));put(level.data(),0x1d0,75);
    put(tables.data(),0xc5c,280);put(tables.data(),0xc58,address(level.data()));put(tables.data(),0xc60,address(level.data()));
}
void early_scenarios(){
    unsigned expected=calls;
    const auto early_call=[&]{mxl::reveal::before_scene(window);require(calls==++expected,"Ready native scene boundary did not reveal once.");};
    require(frames_begun==0 && presentations==0,"Early fixture already rendered a frame.");
    SetLastError(0x2468);early_call();
    require(GetLastError()==0x2468,"Early route changed the caller's last-error.");
    require(frames_begun==0 && presentations==0,"First reveal waited for rendering or presentation.");
    frame();pump();require(calls==expected,"First gameplay frame repeated the early reveal.");
    for(const auto index:{3u,4u}){put(act.data(),0x14,index);early_call();frame();pump();}
    require(calls==expected,"Three-act early sequence repeated a reveal.");
    put(act.data(),0x14,2);mxl::reveal::before_scene(window);
    put(level.data(),0x1d0,137);mxl::reveal::before_scene(window);
    require(calls==expected,"Revisited act or same-act level change repeated early reveal.");
    state.fill(0);early_call();

    flag(0);mxl::reveal::begin_frame(window);mxl::reveal::before_scene(window);
    require(calls==expected,"Early route reentered an active draw.");
    mxl::reveal::end_frame(window,true);early_call();pump();
    require(calls==expected,"Early route did not consume an already queued reveal.");

    flag(0);put(path.data(),0x1c,0);mxl::reveal::before_scene(window);
    require(calls==expected,"Incomplete pre-scene graph triggered reveal.");
    put(path.data(),0x1c,address(room1.data()));
    frame();require(calls==expected,"Not-ready early route disabled deferred execution.");
    pump();require(calls==++expected,"Ready posted fallback was lost after an incomplete early snapshot.");
    flag(0);std::thread wrong_thread([]{mxl::reveal::before_scene(window);});wrong_thread.join();
    mxl::reveal::before_scene(nullptr);require(calls==expected,"Early route accepted a foreign thread or window.");
    put(path.data(),0x1c,1);mxl::reveal::before_scene(window);
    put(path.data(),0x1c,address(room1.data()));put(misc.data(),0x46c,1);mxl::reveal::before_scene(window);
    require(calls==expected,"Early route accepted an invalid or mismatched graph.");put(misc.data(),0x46c,address(act.data()));

    // Leave the native bit clear: an old posted message must still be consumed
    // even when the synchronous callback cannot confirm completion.
    finish=false;frame();early_call();pump();
    require(calls==expected,"Old queued message repeated an unconfirmed early reveal.");
    for(int i=0;i<5;++i){mxl::reveal::before_scene(window);frame();pump();}
    require(calls==expected,"Unconfirmed early callback retried every scene.");finish=true;
    in_game=false;frame();in_game=true;
    frame();put(act.data(),0x14,3);early_call();pump();
    require(calls==expected,"Previous act's queued message survived the new early act.");
    flag(0);frame();flag(1);mxl::reveal::before_scene(window);pump();
    require(calls==expected,"Manual T flag caused an early or queued duplicate.");
    flag(0);nested=true;early_call();nested=false;
    require(calls==expected,"Nested native boundary or pump reentered early reveal.");

    flag(0);frame();throw_original=true;bool caught=false;SetLastError(0x2468);
    try{mxl::reveal::before_scene(window);}catch(const std::runtime_error&){caught=true;}
    require(caught && calls==++expected && GetLastError()==0x2468,"Early exception or caller last-error was lost.");
    throw_original=false;pump();mxl::reveal::before_scene(window);frame();pump();
    require(calls==expected,"Failed early call was repeated by its queued message or next scene.");
    in_game=false;frame();in_game=true;early_call();
}
void posted_scenarios(){
    const auto base=calls;state.fill(0);setup();finish=true;nested=false;throw_original=false;
    in_game=false;frame();pump();require(calls==base,"Menu triggered reveal.");in_game=true;
    put(path.data(),0x1c,0);frame();pump();require(calls==base,"Incomplete loading triggered reveal.");
    put(path.data(),0x1c,address(room1.data()));
    frame();require(calls==base,"Reveal executed in the draw callback.");pump();require(calls==base+1,"Act entry did not reveal.");
    for(int i=0;i<20;++i){frame();pump();}require(calls==base+1,"Already completed act repeated.");
    put(level.data(),0x1d0,137);frame();pump();require(calls==base+1,"Same-act level change repeated reveal.");

    put(act.data(),0x14,3);nested=true;frame();pump();require(calls==base+2,"New act or nested-pump guard failed.");nested=false;
    put(act.data(),0x14,2);frame();pump();require(calls==base+2,"Revisited act repeated reveal.");
    state.fill(0);frame();pump();require(calls==base+3,"New game with reused addresses did not reveal.");

    flag(0);frame();flag(1);pump();require(calls==base+3,"Manual T before queued message duplicated reveal.");
    flag(0);frame();mxl::reveal::begin_frame(window);pump();require(calls==base+3,"Reveal ran during a nested draw.");
    mxl::reveal::end_frame(window,true);pump();require(calls==base+4,"Deferred nested draw never recovered.");
    flag(0);frame();in_game=false;pump();require(calls==base+4,"Quit before dispatch used stale state.");frame();in_game=true;
    frame();put(act.data(),0x14,4);pump();require(calls==base+4,"Stale act message was executed.");
    frame();pump();require(calls==base+5,"Latest act was not retried.");

    flag(0);std::thread wrong_thread([]{frame();});wrong_thread.join();pump();require(calls==base+5,"Worker thread queued reveal.");
    put(path.data(),0x1c,1);frame();pump();require(calls==base+5,"Invalid pointer was accepted.");put(path.data(),0x1c,address(room1.data()));
    put(misc.data(),0x46c,1);frame();pump();require(calls==base+5,"Mismatched act graph was accepted.");put(misc.data(),0x46c,address(act.data()));

    finish=false;frame();pump();require(calls==base+6,"Unconfirmed callback not attempted.");
    for(int i=0;i<20;++i){frame();pump();}require(calls==base+6,"Unconfirmed callback repeated every frame.");
    in_game=false;frame();in_game=true;finish=true;
    frame();MSG msg{};require(PeekMessageW(&msg,window,message,message,PM_REMOVE)!=0,"Missing exception test message.");
    throw_original=true;bool caught=false;
    try{mxl::reveal::window_message(window,message,msg.wParam,true);}catch(const std::runtime_error&){caught=true;}
    require(caught && calls==base+7,"Original exception lost.");throw_original=false;
    in_game=false;frame();in_game=true;frame();pump();require(calls==base+8,"Busy guard did not recover after unwind.");
}
}
int wmain(int argc,wchar_t** argv){
    try {
        require(argc==2 || argc==3,"Pass a test directory and optional logging or before-scene mode.");
        const bool logging_off=argc==3 && !wcscmp(argv[2],L"--logging-off");
        const bool before_scene_unsupported=argc==3 && !wcscmp(argv[2],L"--before-scene-unsupported");
        require(argc==2 || logging_off || before_scene_unsupported || !wcscmp(argv[2],L"--logging-on"),"Unknown test mode.");
        if(logging_off){
            // Run a copy named Game.exe in an isolated directory to exercise the
            // actual production startup default, not the test-directory bypass.
            wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
            require(std::filesystem::path(exe).filename()==L"Game.exe","Off test must be named Game.exe.");
            require(!mxl::diag::start(nullptr) && !mxl::diag::enabled() && !mxl::diag::audio_enabled(),"Recording should be off.");
            require(mxl::diag::audio_hook_count()==0,"Off startup installed audio hooks.");
            require(!std::filesystem::exists(std::filesystem::path(exe).parent_path()/L"mxl-diagnostics"),"Off startup created a log directory.");
        }else if(argc==3 && !wcscmp(argv[2],L"--logging-on")){
            require(mxl::diag::start(nullptr) && mxl::diag::enabled(),"Explicit INI opt-in did not start recording.");
        }else require(mxl::diag::start(nullptr,argv[1]),"Start diagnostics.");
        setup();owner=GetCurrentThreadId();
        WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"MXLAutoRevealTest";
        require(RegisterClassW(&wc)!=0,"Register test window.");
        window=CreateWindowW(wc.lpszClassName,L"",0,0,0,1,1,HWND_MESSAGE,nullptr,wc.hInstance,nullptr);
        require(window!=nullptr,"Create test window.");
        const auto sigma_base=reinterpret_cast<uintptr_t>(sigma.data());
        require(before_scene_unsupported?mxl::reveal::start(window,sigma_base,reveal_mock):
            mxl::reveal::start(window,sigma_base,reveal_mock,true),"Start scheduler.");
        message=RegisterWindowMessageW(L"MXL.SmoothMotionDX12.ActEntryReveal.1");
        if(before_scene_unsupported){
            mxl::reveal::before_scene(window);require(calls==0,"Unsupported native boundary invoked reveal.");
        }else early_scenarios();
        posted_scenarios();
        const auto before_stop=calls;flag(0);mxl::diag::stop(true);mxl::reveal::before_scene(window);
        require(calls==before_stop+(before_scene_unsupported?0:1),"Stopping recording changed the guarded early route.");
        frame();pump();require(calls==before_stop+1,"Stopping recording disabled or repeated automatic reveal.");
        DestroyWindow(window);
        std::cout<<(before_scene_unsupported?"PASS: unsupported pre-scene boundary retains posted reveal. ":
            "PASS: pre-scene reveal before first presentation, three-act transitions and guarded fallback. ")
            <<"Game-thread/per-act/reused-session checks, manual T race, stale messages, nested draws, invalid pointers, failure/unwind cleanup and logging-independent operation.\n";
        return 0;
    }catch(const std::exception& error){mxl::diag::stop(true);std::cerr<<error.what()<<"\n";return 1;}
}
