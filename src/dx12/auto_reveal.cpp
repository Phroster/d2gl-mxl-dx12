#include "auto_reveal.h"
#include "diagnostics.h"
#include <atomic>

namespace mxl::reveal {
namespace {
struct Entry {
    uintptr_t state=0, player=0, act=0, misc=0;
    uint32_t index=0;
    bool operator==(const Entry&) const = default;
};
struct Snapshot { Entry entry; bool revealed=false; };
struct Controller {
    // Published once by renderer initialization; all later mutable state belongs
    // to the window/game thread. No room pointers survive a message boundary.
    std::atomic<bool> ready{false};
    HWND window=nullptr;
    DWORD thread=0;
    UINT message=0;
    uintptr_t sigma=0;
    diag::RevealRootFn reveal=nullptr;
    bool drawing=false, pending=false, busy=false, failed=false;
    int wait_reason=-1;
    WPARAM token=0;
    Entry queued{}, failure{};
} controller;
uint32_t field(uintptr_t base, size_t offset=0) {
    return *reinterpret_cast<const uint32_t*>(base+offset);
}
bool snapshot(Snapshot& out,int* reason=nullptr) noexcept {
    // Read-only checks, all on the game's own thread. A transitional or unknown
    // layout fails closed instead of marking the act revealed prematurely.
    int stage=2;
    __try {
        const auto sb=controller.sigma;
        Entry entry;
        entry.state=field(sb,0x3fdecc);
        const auto player_global=field(sb,0x3fa130),tables_global=field(sb,0x3fa958);
        const auto layer_global=field(sb,0x3fa174);
        if(!entry.state || !player_global || !tables_global || !layer_global){if(reason)*reason=stage;return false;}
        stage=3;
        entry.player=field(player_global);
        const auto tables=field(tables_global);
        if(!entry.player || !tables || !field(layer_global) || field(entry.player)!=0){if(reason)*reason=stage;return false;}
        stage=4;
        entry.act=field(entry.player,0x1c);
        const auto path=field(entry.player,0x2c);
        if(!entry.act || !path){if(reason)*reason=stage;return false;}
        stage=5;
        entry.index=field(entry.act,0x14);
        entry.misc=field(entry.act,0x48);
        if(entry.index>4 || !entry.misc || field(entry.misc,0x46c)!=entry.act){if(reason)*reason=stage;return false;}
        stage=6;
        const auto room1=field(path,0x1c);
        if(!room1){if(reason)*reason=stage;return false;}
        stage=7;
        const auto room2=field(room1,0x10);
        if(!room2 || field(room2,0x30)!=room1){if(reason)*reason=stage;return false;}
        stage=8;
        const auto level=field(room2,0x58);
        if(!level || field(level,0x1b4)!=entry.misc || !field(level,0x10)){if(reason)*reason=stage;return false;}
        stage=9;
        const auto id=field(level,0x1d0),count=field(tables,0xc5c);
        if(!id || count>4096 || id>=count || !field(tables,0xc58) || !field(tables,0xc60)){if(reason)*reason=stage;return false;}
        stage=10;
        const auto flag=field(entry.state,0x1c+entry.index*4);
        if(flag>1){if(reason)*reason=stage;return false;}
        out={entry,flag==1};
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { if(reason)*reason=100+stage;return false; }
}
bool owns(HWND window) noexcept {
    return controller.ready.load(std::memory_order_acquire) && window==controller.window &&
        GetCurrentThreadId()==controller.thread;
}
void cancel() noexcept { controller.pending=false;controller.failed=false; }
void waiting(int reason) noexcept {
    if(controller.wait_reason!=reason){controller.wait_reason=reason;diag::note("auto_reveal_wait_reason",reason);}
}
void invoke(const Entry& entry) {
    const auto error=GetLastError();
    controller.busy=true;
    // If the original routine fails or returns without setting its flag, never
    // keep retrying the expensive call every frame. Manual T remains untouched.
    controller.failed=true;controller.failure=entry;
    diag::note("auto_reveal_start_act",entry.index+1);
    __try {
        controller.reveal();
        Snapshot after;
        const bool complete=snapshot(after) && after.entry==entry && after.revealed;
        controller.failed=!complete;
        diag::note(complete?"auto_reveal_complete_act":"auto_reveal_unconfirmed_act",entry.index+1);
    } __finally { controller.busy=false;SetLastError(error); }
}
}
bool start(HWND window, uintptr_t sigma, diag::RevealRootFn reveal) {
    if(controller.ready.load(std::memory_order_acquire))return true;
    DWORD process=0;
    const auto thread=GetWindowThreadProcessId(window,&process);
    if(!window || !sigma || !reveal || !thread || process!=GetCurrentProcessId())return false;
    const auto message=RegisterWindowMessageW(L"MXL.SmoothMotionDX12.ActEntryReveal.1");
    if(!message)return false;
    controller.window=window;controller.thread=thread;controller.sigma=sigma;
    controller.reveal=reveal;controller.message=message;
    controller.ready.store(true,std::memory_order_release);
    diag::note("auto_reveal_ready");return true;
}
void begin_frame(HWND window) noexcept {
    if(owns(window))controller.drawing=true;
}
void end_frame(HWND window,bool in_game) noexcept {
    if(!owns(window))return;
    controller.drawing=false;
    if(controller.busy)return;
    if(!in_game){waiting(1);cancel();return;}
    Snapshot now;int reason=0;
    if(!snapshot(now,&reason)){waiting(reason);cancel();return;}
    waiting(0);
    if(now.revealed){cancel();return;}
    if(controller.failed && controller.failure==now.entry)return;
    if(controller.pending && controller.queued==now.entry)return;
    controller.queued=now.entry;
    if(!++controller.token)++controller.token;
    controller.pending=true;
    if(!PostMessageW(window,controller.message,controller.token,0)){
        controller.pending=false;diag::note("auto_reveal_post_failed",GetLastError());
    }
}
bool window_message(HWND window,UINT message,WPARAM token,bool in_game) {
    if(!owns(window) || message!=controller.message)return false;
    // Only our queued message, never a synchronous sent/nested window call.
    if(InSendMessageEx(nullptr)!=ISMEX_NOSEND || !controller.pending || token!=controller.token)return true;
    controller.pending=false;
    if(controller.drawing || controller.busy || !in_game)return true;
    Snapshot now;
    if(!snapshot(now) || now.revealed || !(now.entry==controller.queued))return true;
    invoke(now.entry);return true;
}
}
