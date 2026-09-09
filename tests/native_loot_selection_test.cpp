// SPDX-License-Identifier: GPL-3.0-or-later
// Execute the production hook against controlled game callbacks. In an empty
// scene even the input pointers may be unavailable: only native selection runs.
#include <windows.h>
#include <glm/vec2.hpp>
#include "native_loot_cells.h"
#include "native_loot_pickup.h"
#include <iostream>
#include <stdexcept>
#include <chrono>

namespace fixture {
namespace d2 {
enum class UnitType { Player, Item };
struct UnitAny { UnitType dwType=UnitType::Item; struct { unsigned dwMode=3; } v110; };
unsigned panels=0,alt=0,c1=0,c2=0,c3=0;
int mx=100,my=100;
unsigned *screen_shift=&panels,*is_alt_clicked=&alt,*cursor_state1=&c1,*cursor_state2=&c2,*cursor_state3=&c3;
int *mouse_x=&mx,*mouse_y=&my;
UnitAny unit,nativeUnit{UnitType::Player},*selected=nullptr,*nativeResult=nullptr;
UnitAny* getSelectedUnit() { return selected; }
}
using namespace mxl::native_loot;
unsigned frameRevision=1,inputRevision=0,locked=0,action=0;
unsigned *selectionLocked=&locked,*cursorAction=&action;
unsigned pickCount=0,pickDrawn=100;
uint64_t selectionCalls=0,hoverChecks=0,hoverSelections=0,panelHoverSelections=0;
bool hoveredValid=false,carried=false,alive=true;
SelectionCache selectionCache;
std::array<GroundEntry,60> pickItems{};
GroundEntry hovered{};
unsigned inventoryQueries=0,inputQueries=0,lookups=0,nativeCalls=0;
unsigned GetTickCount() { return 100; }
short GetKeyState(int) { return 0; }
bool carryingItem() { ++inventoryQueries; return carried; }
void originalSelection() { ++nativeCalls; d2::selected=d2::nativeResult; }
bool inputAllowed(int,int) { ++inputQueries; return !carryingItem() && !locked; }
View view() { return {3,1280,720,0,false}; }
d2::UnitAny* resolve(const GroundEntry&) { ++lookups; return alive?&d2::unit:nullptr; }
Appearance lookFor(d2::UnitAny*) { return appearance(P_Supply); }
glm::ivec2 anchor(d2::UnitAny*,bool) { return {100,120}; }
bool onItemLabel(const GroundEntry&,int,int,int,int,unsigned) { return false; }
int worldMouse(int*,int*) { return 0; }
bool selectable(d2::UnitAny*,int,int,int) { return alive && !carried; }
void selectNative(d2::UnitAny* unit) { d2::selected=unit; }

#include "modules/native_loot_selection.inl"

void require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
void benchmark() {
    constexpr unsigned polls=1000000;
    const auto begin=std::chrono::steady_clock::now();
    for(unsigned i=0;i<polls;++i) { if(i%1000==0) ++frameRevision; updateSelection(); }
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    std::cout << "Empty-scene replay: polls=" << polls << " native_calls=" << nativeCalls
        << " inventory_queries=" << inventoryQueries << " input_queries=" << inputQueries
        << " elapsed_ms=" << ms << " (synthetic callbacks, not a live frame-time measurement)\n";
}
void scenarios() {
    // Do not touch extension inputs during an empty native poll, even when
    // coming from an old custom selection and a valid cached input key.
    d2::screen_shift=nullptr; selectionLocked=nullptr; cursorAction=nullptr;
    hoveredValid=true; selectionCache.valid=true; d2::nativeResult=&d2::nativeUnit;
    updateSelection();
    require(nativeCalls==1 && d2::selected==&d2::nativeUnit,"Empty scene did not preserve native selection.");
    require(!hoveredValid && !selectionCache.valid,"Empty scene retained a stale custom hover.");
    require(!inventoryQueries && !inputQueries && !lookups,"Empty scene queried extension state.");
    d2::screen_shift=&d2::panels; selectionLocked=&locked; cursorAction=&action;
    d2::nativeResult=nullptr;
    // A newly visible item must be selectable immediately, even if the input
    // key is otherwise identical to the key from before the empty scene.
    pickItems[0]={1,0,1,1,0,0,view()}; pickCount=1;
    updateSelection();
    require(hoveredValid && d2::selected==&d2::unit,"New loot did not re-enable effect pickup.");
    const auto calls=nativeCalls;
    updateSelection();
    require(nativeCalls==calls && hoveredValid,"Repeated valid custom hover was not retained.");
    alive=false;updateSelection();
    require(!hoveredValid && !d2::selected && nativeCalls==calls+1,"Removed loot left stale selection.");
    pickCount=0;updateSelection();
    alive=true;pickCount=1;updateSelection();
    require(hoveredValid && d2::selected==&d2::unit,"Pickup did not recover after the last item disappeared.");
    // Inventory can change without a window event: a retained custom target
    // must be released immediately, even with an otherwise identical key.
    carried=true; updateSelection();
    require(!hoveredValid && !d2::selected,"Cursor-held item did not suppress effect pickup.");
    carried=false;++frameRevision;updateSelection();
    require(hoveredValid && d2::selected==&d2::unit,"New draw did not resume pickup after releasing cursor item.");
    d2::mx=1100;updateSelection();
    require(!hoveredValid && !d2::selected,"Moving away retained a custom loot target.");
    d2::mx=100;updateSelection();
    require(hoveredValid && d2::selected==&d2::unit,"Moving onto loot did not select it immediately.");
    pickCount=0;carried=false;selectionCache={};
    nativeCalls=inventoryQueries=inputQueries=lookups=0;
    benchmark();
    require(nativeCalls==1000000 && !inventoryQueries && !inputQueries && !lookups,
        "Repeated empty input polling performed custom game queries.");
    // Sixty visible effects, with the mouse away from them. The native loop
    // still runs on every poll, but unchanged failed hover attempts must not
    // repeatedly fetch the player inventory between rendered frames.
    for(auto& item:pickItems) item=pickItems[0];
    pickCount=60;d2::mx=1100;d2::my=600;
    nativeCalls=inventoryQueries=inputQueries=lookups=0;
    for(unsigned i=0;i<1000000;++i) {
        if(i%1000==0) ++frameRevision;
        updateSelection();
    }
    std::cout << "Visible-loot replay: polls=1000000 native_calls=" << nativeCalls
        << " inventory_queries=" << inventoryQueries << " input_queries=" << inputQueries
        << " unit_lookups=" << lookups << " (synthetic callbacks, not live game timing)\n";
    require(nativeCalls==1000000 && inventoryQueries==1000 && inputQueries==1000 && lookups==60000,
        "Unchanged missed hovers repeated inventory queries between draw frames.");
}
}
int main() {
    try {
        fixture::scenarios();
        std::cout << "PASS: empty/visible loot input, appearance/removal, cached hover and held-item gates.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
