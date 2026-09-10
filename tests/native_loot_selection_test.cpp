// SPDX-License-Identifier: GPL-3.0-or-later
// Execute the production hook against controlled game callbacks. In an empty
// scene even the input pointers may be unavailable: only native selection runs.
#include <windows.h>
#include <glm/vec2.hpp>
#include "native_loot_cells.h"
#include "native_loot_pickup.h"
#include "world_objects.h"
#include "world_object_groups.h"
#include "diagnostics.h"
#include <iostream>
#include <stdexcept>
#include <chrono>

namespace fixture {
namespace d2 {
enum class UnitType { Player, Item, Object };
struct UnitAny { UnitType dwType=UnitType::Item; struct { unsigned dwMode=3,dwUnitId=1; } v110; };
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
bool objectIndicatorsEnabled=true,objectsPainted=false,hoveredObjectValid=false,objectAlive=true;
ObjectIndicators objectIndicators;
ObjectGroups objectGroups;
ObjectNameCoverage objectNameCoverage;
std::array<HoverLabel,object_label_limit> oldObjectLabels{};
unsigned oldObjectLabelCount=0,objectLookups=0;
GroundEntry hoveredObject{};
d2::UnitAny objectUnit{d2::UnitType::Object,{0,17}};
d2::UnitAny urnUnit{d2::UnitType::Object,{0,18}},secondUrn{d2::UnitType::Object,{0,19}};
SelectionCache selectionCache;
std::array<GroundEntry,60> pickItems{};
GroundEntry hovered{};
unsigned inventoryQueries=0,inputQueries=0,lookups=0,nativeCalls=0;
unsigned GetTickCount() { return 100; }
short GetKeyState(int) { return 0; }
bool carryingItem() { ++inventoryQueries; return carried; }
void originalSelection() { ++nativeCalls; d2::selected=d2::nativeResult; }
bool inputAllowed(int x,int y) { ++inputQueries; return !carryingItem() && !locked && !d2::alt
    && world_input_point(d2::panels,1280,720,x,y) && GetTickCount()-pickDrawn<=120; }
View view() { return {3,1280,720,0,false}; }
d2::UnitAny* resolve(const GroundEntry&) { ++lookups; return alive?&d2::unit:nullptr; }
Appearance lookFor(d2::UnitAny*) { return appearance(P_Supply); }
glm::ivec2 anchor(d2::UnitAny*,bool) { return {100,120}; }
bool onItemLabel(const GroundEntry&,int,int,int,int,unsigned) { return false; }
int worldMouse(int*,int*) { return 0; }
bool selectable(d2::UnitAny*,int,int,int) { return alive && !carried; }
void selectNative(d2::UnitAny* unit) { d2::selected=unit; }
d2::UnitAny* resolveObject(const GroundEntry& id) {
    ++objectLookups;
    if(!carried && id.id==18 && id.seed==94 && urnUnit.v110.dwMode==0) return &urnUnit;
    if(!carried && id.id==19 && id.seed==95 && secondUrn.v110.dwMode==0) return &secondUrn;
    return objectAlive && objectUnit.v110.dwMode==0 && id.id==17 && id.seed==93 && !carried?&objectUnit:nullptr;
}

#include "modules/native_object_selection.inl"
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
    // Object-only scenes must activate extension input without dropped loot.
    pickCount=0;d2::mx=400;d2::my=294;objectsPainted=true;++frameRevision;
    ObjectIndicator e{};e.identity={17,4,93,1,0,0,view()};e.x=400;e.y=300;
    e.look={ObjectKind::Shrine,2,3,1,24};e.name[0]=L'S';objectIndicators.remember(e);
    objectGroups.rebuild(objectIndicators);
    updateSelection();
    require(hoveredObjectValid && !hoveredValid && d2::selected==&objectUnit,"Shrine effect cannot be selected without item loot.");
    auto objectCalls=nativeCalls;updateSelection();
    require(nativeCalls==objectCalls && hoveredObjectValid,"Object hover flickers between identical polls.");
    oldObjectLabels[0]={e.identity,{-70,-80,70,-50},100,true};oldObjectLabelCount=1;
    objectNameCoverage.add(objectIndicators,objectGroups.markers.entries[0],0);
    require(suppressObjectHoverLabel(&objectUnit),"Visible permanent object name retained a duplicate native hover name.");
    require(!suppressObjectHoverLabel(nullptr) && !suppressObjectHoverLabel(&d2::unit)
        && !suppressObjectHoverLabel(&d2::nativeUnit),"Object-name suppression affected another unit type.");
    oldObjectLabelCount=0;
    require(!suppressObjectHoverLabel(&objectUnit),"Unlabelled or budget-limited object lost its native name.");
    oldObjectLabelCount=1;oldObjectLabels[0].valid=false;
    require(!suppressObjectHoverLabel(&objectUnit),"Unpainted object name suppressed native hover text.");
    oldObjectLabels[0].valid=true;oldObjectLabels[0].item.view.width=640;
    require(!suppressObjectHoverLabel(&objectUnit),"Old viewport's object label suppressed native hover text.");
    oldObjectLabels[0].item.view=view();objectIndicatorsEnabled=false;
    require(!suppressObjectHoverLabel(&objectUnit),"Disabled object labels suppressed native hover text.");
    objectIndicatorsEnabled=true;
    d2::mx=450;d2::my=230;updateSelection();
    require(hoveredObjectValid && d2::selected==&objectUnit,"Displaced object name cannot be selected.");
    beforeObjectClick(450,230);require(d2::selected==&objectUnit,"Valid object label click lost native target.");
    objectUnit.v110.dwMode=2;beforeObjectClick(450,230);
    require(!d2::selected && !hoveredObjectValid,"Consumed shrine retained a clickable target.");
    require(!suppressObjectHoverLabel(&objectUnit),"Consumed object suppressed native hover text.");
    objectUnit.v110.dwMode=0;++frameRevision;updateSelection();
    require(hoveredObjectValid,"Reusable valid target did not recover.");
    objectAlive=false;updateSelection();require(!hoveredObjectValid && !d2::selected,"Removed object retained cached selection.");
    objectAlive=true;++frameRevision;updateSelection();
    carried=true;beforeObjectClick(450,230);require(!d2::selected,"Object extension intercepted held inventory item.");
    carried=false;d2::alt=1;++frameRevision;updateSelection();require(!hoveredObjectValid,"Object extension ignored Alt gate.");
    d2::alt=0;d2::panels=3;++frameRevision;updateSelection();require(!hoveredObjectValid,"Object target covered both inventory panels.");
    d2::panels=0;oldObjectLabels[0].item.seed=99;++frameRevision;updateSelection();
    require(!hoveredObjectValid,"Recycled object identity inherited stale label.");
    require(!suppressObjectHoverLabel(&objectUnit),"Recycled object inherited another object's name suppression.");
    oldObjectLabels[0].item.seed=93;oldObjectLabels[0].painted=unsigned(100-121);++frameRevision;updateSelection();
    require(!hoveredObjectValid,"Expired object label stayed clickable.");
    require(!suppressObjectHoverLabel(&objectUnit),"Expired object label suppressed native hover text.");
    d2::mx=400;d2::my=294;d2::nativeResult=&d2::nativeUnit;++frameRevision;updateSelection();
    require(!hoveredObjectValid && d2::selected==&d2::nativeUnit,"Object cue stole native player/monster selection.");
    d2::nativeResult=nullptr;objectsPainted=false;++frameRevision;updateSelection();
    require(!hoveredObjectValid,"Unpainted object became clickable.");
    objectsPainted=true;d2::mx=1100;d2::my=600;
    nativeCalls=inventoryQueries=inputQueries=objectLookups=0;
    for(unsigned i=0;i<1000000;++i) { if(i%1000==0) ++frameRevision;updateSelection(); }
    require(nativeCalls==1000000 && inventoryQueries==1000 && objectLookups==0,
        "Missed object hovers resolve game objects or poll inventory between frames.");
    std::cout << "Object input: glow/name clicks, consumed/removed/recycled targets, UI/held-item gates and 1000000 missed polls passed.\n";
    // The shared label targets its named leader at the GROUP centre. The
    // original sprites/effects continue to select each constituent separately.
    objectIndicators.clear();objectIndicators.remember(e);
    auto urn=e;urn.identity.id=18;urn.identity.seed=94;urn.x=448;urn.y=314;
    urn.look={ObjectKind::Container,1,5,1,4};std::wcscpy(urn.name.data(),L"Urn");objectIndicators.remember(urn);
    auto urn2=urn;urn2.identity.id=19;urn2.identity.seed=95;urn2.x=432;urn2.y=310;objectIndicators.remember(urn2);
    objectGroups.rebuild(objectIndicators);
    auto group=objectGroups.markers.entries[0];
    require(group.members==3 && group.identity.id==17,"Mixed group's named target is not its important member.");
    oldObjectLabels[0]={group.identity,{-70,-80,70,-50},100,true};oldObjectLabelCount=1;
    objectNameCoverage.clear();objectNameCoverage.add(objectIndicators,group,0);
    d2::mx=group.x+69;d2::my=group.y-51;++frameRevision;updateSelection();
    require(hoveredObjectValid && d2::selected==&objectUnit,"Shared count label is not clickable at its displayed position.");
    beforeObjectClick(d2::mx,d2::my);require(d2::selected==&objectUnit,"Group name did not preserve its one normal native target.");
    d2::nativeResult=&urnUnit;++frameRevision;updateSelection();
    require(d2::selected==&objectUnit,"Native urn behind a mixed group name stole its named target.");
    d2::nativeResult=nullptr;
    require(suppressObjectHoverLabel(&objectUnit) && suppressObjectHoverLabel(&urnUnit)
        && suppressObjectHoverLabel(&secondUrn),"Group members retained duplicate hover names.");
    objectGroups.clear();objectsPainted=false;
    require(suppressObjectHoverLabel(&objectUnit) && suppressObjectHoverLabel(&urnUnit)
        && suppressObjectHoverLabel(&secondUrn),"Native hover names reappeared before the next world pass finished.");
    objectGroups.rebuild(objectIndicators);objectsPainted=true;
    d2::mx=448;d2::my=314;d2::nativeResult=&urnUnit;++frameRevision;updateSelection();
    require(d2::selected==&urnUnit,"Group stole a native click on an individual urn.");
    d2::nativeResult=nullptr;++frameRevision;updateSelection();
    require(hoveredObjectValid && d2::selected==&urnUnit,"Shared glow cannot select an individual urn.");
    beforeObjectClick(d2::mx,d2::my);require(d2::selected==&urnUnit,"Group member lost its normal click.");
    objectUnit.v110.dwMode=2;objectIndicators.clear();objectIndicators.remember(urn);objectIndicators.remember(urn2);
    objectGroups.rebuild(objectIndicators);group=objectGroups.markers.entries[0];
    require(group.members==2 && group.identity.id==18,"Opening the leader left a stale group count or target.");
    oldObjectLabels[0]={group.identity,{-70,-80,70,-50},100,true};
    objectNameCoverage.clear();objectNameCoverage.add(objectIndicators,group,0);
    d2::mx=group.x;d2::my=group.y-60;++frameRevision;updateSelection();
    require(d2::selected==&urnUnit && !suppressObjectHoverLabel(&objectUnit),"Shrunken group retained its opened member.");
    urnUnit.v110.dwMode=2;objectIndicators.clear();objectIndicators.remember(urn2);objectGroups.rebuild(objectIndicators);
    oldObjectLabelCount=0;d2::mx=urn2.x;d2::my=urn2.y;++frameRevision;updateSelection();
    require(d2::selected==&secondUrn && !suppressObjectHoverLabel(&secondUrn),"Last urn lost its individual native name or click.");
    objectIndicators.clear();objectGroups.clear();objectsPainted=false;++frameRevision;updateSelection();
    require(!hoveredObjectValid && !d2::selected,"Empty group retained an input target.");
    // Small native glints form a shared pulse too; its centre/outer edge must
    // not have dead zones between their smaller individual hitboxes.
    urnUnit.v110.dwMode=secondUrn.v110.dwMode=0;
    urn.look=urn2.look={ObjectKind::Usable,1,5,0,1};urn.x=400;urn.y=300;urn2.x=464;urn2.y=332;
    objectIndicators.remember(urn);objectIndicators.remember(urn2);objectGroups.rebuild(objectIndicators);objectsPainted=true;
    group=objectGroups.markers.entries[0];
    d2::mx=group.x;d2::my=group.y;++frameRevision;updateSelection();
    require(hoveredObjectValid && d2::selected==&urnUnit,"Shared glow has an unclickable centre between small glints.");
    d2::mx=group.x-60;d2::my=group.y+20;++frameRevision;updateSelection();
    require(hoveredObjectValid && d2::selected==&urnUnit,"Shared glow has an unclickable outer edge.");
    beforeObjectClick(d2::mx,d2::my);require(d2::selected==&urnUnit,"Shared glow edge rejected a valid normal click.");
    std::cout << "Grouped object input: centred label, individual native/glow clicks, duplicate-name suppression and shrinking groups passed.\n";
}
}
int main() {
    try {
        fixture::scenarios();
        std::cout << "PASS: empty/visible loot input, appearance/removal, cached hover and held-item gates.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
