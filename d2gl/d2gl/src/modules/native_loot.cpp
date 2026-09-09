// SPDX-License-Identifier: GPL-3.0-or-later
#include "pch.h"
#include "native_loot.h"
#include "d2/common.h"
#include "helpers.h"
#include "native_loot_cells.h"
#include "native_loot_assets.h"
#include "native_loot_rules.h"
#include "native_loot_layer.h"
#include "native_loot_pickup.h"
#include "option/menu.h"
#include "hd_text.h"
#include <detours/detours.h>
#include <wincrypt.h>
#include <cstdio>
#include <chrono>
#include "stb/stb_image.h"

namespace d2gl::modules::NativeLoot {
namespace {
using WorldDraw = uint32_t(__fastcall*)(d2::UnitAny*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
using Normalize = void(__stdcall*)(void*, d2::CellFile**, const char*, int, int, int);
WorldDraw original = nullptr;
using UnitCoordinate = int(__stdcall*)(d2::UnitAny*);
using Project = void(__stdcall*)(int,int,int,int*,int*,int);
UnitCoordinate worldX=nullptr,worldY=nullptr;
Project project=nullptr;
int *cameraX=nullptr,*cameraY=nullptr,*viewShift=nullptr;
thread_local d2::UnitAny* current = nullptr;
thread_local bool painted = false;
struct Animation { std::vector<uint8_t> bytes,glowBytes; d2::CellFile *file=nullptr,*glow=nullptr; };
// Native hardware caches refer back to these bytes. The fixed catalogue of
// animations lives through graphics/DLL teardown; the process owns its lifetime.
std::array<Animation,mxl::native_loot::ProfileCount>* animations = nullptr;
std::array<Animation,24>* landingAnimations = nullptr;
std::array<uint64_t,mxl::native_loot::ProfileCount> profileDraws{};
bool tried = false, active = false, wasInGame = false;
uint32_t tick = 0;
uint64_t groundCalls = 0, effectDraws = 0, limited = 0, floorPasses = 0, bloomDraws = 0;
uint64_t spriteDraws=0;
uint64_t landingDraws=0;
uint64_t worldCalls=0,captures=0,viewRejected=0,lookupRejected=0,identityRejected=0;
uint64_t secondTableMatches=0;
unsigned sampleFrame=0;
unsigned playerLevel=0;
int lastX=0,lastY=0;
mxl::native_loot::Budget budget;
mxl::native_loot::GroundQueue queue;
mxl::native_loot::PulseHistory pulseHistory;
bool floorPainted=false;
std::filesystem::path directory;
using SelectionUpdate = void(__stdcall*)();
using Selectable = int(__stdcall*)(d2::UnitAny*,int,int,int);
using CursorItem = d2::UnitAny*(__stdcall*)(void*);
using ItemName = void(__stdcall*)(d2::UnitAny*,wchar_t*,uint32_t);
SelectionUpdate originalSelection=nullptr;
Selectable selectable=nullptr;
CursorItem cursorItem=nullptr;
ItemName itemName=nullptr;
void *selectAddress=nullptr,*worldMouseAddress=nullptr;
uint32_t *selectionLocked=nullptr,*cursorAction=nullptr;
bool pickupActive=false,hoveredValid=false;
DWORD gameThread=0;
uint32_t pickDrawn=0;
unsigned pickCount=0;
std::array<mxl::native_loot::GroundEntry,60> pickItems{};
mxl::native_loot::GroundEntry hovered{};
mxl::native_loot::HoverLabel label;
uint64_t selectionCalls=0,hoverChecks=0,hoverSelections=0,hoverLabels=0,pickClicks=0;
uint64_t panelHoverSelections=0,panelPickClicks=0;
bool panelNameReported=false;
uint32_t hoverGate=0;
uint32_t frameRevision=0,inputRevision=0;
mxl::native_loot::SelectionCache selectionCache;
struct NameEntry {
    mxl::native_loot::GroundEntry item{};
    std::array<wchar_t,512> text{};
    uint32_t flags=0,quality=0,updated=0,lastSeen=0;
    bool used=false;
};
std::array<NameEntry,128> names{};
std::array<mxl::native_loot::HoverLabel,60> groundLabels{};
unsigned groundLabelCount=0;
bool labelsPainted=false;
uint64_t permanentLabels=0,nameFormats=0,fallingLabels=0,emptyNameRetries=0;
using StartupClock=std::chrono::steady_clock;
double startupTotalMs=0,startupHashMs=0,startupUnpackMs=0,startupNormalizeMs=0;
unsigned startupAssets=0;
double startupElapsed(StartupClock::time_point start) {
    return std::chrono::duration<double,std::milli>(StartupClock::now()-start).count();
}

// Verified 1.13c native ABIs: selection takes EAX; mouse projection takes
// ECX=&x, EAX=&y. Both return without stack arguments and preserve EBX/ESI/EDI.
__declspec(naked) void __fastcall selectNative(d2::UnitAny*) {
    __asm { mov eax,ecx }
    __asm { jmp dword ptr [selectAddress] }
}
__declspec(naked) int __fastcall worldMouse(int*,int*) {
    __asm { mov eax,edx }
    __asm { jmp dword ptr [worldMouseAddress] }
}

void report(const char* status)
{
    if (directory.empty()) return;
    const auto path = directory / ("mxl-native-loot-" + std::to_string(GetCurrentProcessId()) + ".log");
    if (FILE* f = _wfopen(path.c_str(), L"a")) {
        std::fprintf(f,"%s ground_calls=%llu effect_draws=%llu limited=%llu floor_passes=%llu bloom_draws=%llu\n",status,groundCalls,effectDraws,limited,floorPasses,bloomDraws);
        std::fprintf(f,"  world_calls=%llu captures=%llu queued=%u visible=%u view_rejected=%llu lookup_rejected=%llu identity_rejected=%llu second_table_matches=%llu last_xy=%d,%d\n",
            worldCalls,captures,queue.capturedCount,queue.visibleCount,viewRejected,lookupRejected,identityRejected,secondTableMatches,lastX,lastY);
        std::fprintf(f,"  character_level=%u native_sprite_draws=%llu landing_pulse_draws=%llu\n",playerLevel,spriteDraws,landingDraws);
        std::fprintf(f,"  native_pickup=%d selection_calls=%llu hover_checks=%llu hover_selections=%llu name_draws=%llu user_pick_clicks=%llu gate_mask=%u\n",
            pickupActive,selectionCalls,hoverChecks,hoverSelections,hoverLabels,pickClicks,hoverGate);
        std::fprintf(f,"  panel_hover_selections=%llu panel_pick_clicks=%llu\n",panelHoverSelections,panelPickClicks);
        std::fprintf(f,"  permanent_labels=%llu name_formats=%llu label_scales=%.2f,%.2f,%.2f,%.2f effect_x_scales=%.3f,%.3f,%.3f,%.3f effect_y_scales=%.3f,%.3f,%.3f,%.3f\n",
            permanentLabels,nameFormats,mxl::native_loot::ground_label_scale(1),mxl::native_loot::ground_label_scale(2),
            mxl::native_loot::ground_label_scale(3),mxl::native_loot::ground_label_scale(4),
            mxl::native_loot::spectacle_scale(1,true),mxl::native_loot::spectacle_scale(2,true),
            mxl::native_loot::spectacle_scale(3,true),mxl::native_loot::spectacle_scale(4,true),
            mxl::native_loot::spectacle_scale(1),mxl::native_loot::spectacle_scale(2),
            mxl::native_loot::spectacle_scale(3),mxl::native_loot::spectacle_scale(4));
        std::fprintf(f,"  falling_labels=%llu empty_name_retries=%llu\n",fallingLabels,emptyNameRetries);
        std::fprintf(f,"  startup_ms=%.3f hash_ms=%.3f unpack_ms=%.3f normalize_ms=%.3f embedded_assets=%u\n",
            startupTotalMs,startupHashMs,startupUnpackMs,startupNormalizeMs,startupAssets);
        for(unsigned i=1;i<profileDraws.size();++i) if(profileDraws[i])
            std::fprintf(f,"  effect=%s draws=%llu\n",mxl::native_loot::profiles[i].name,profileDraws[i]);
        std::fclose(f);
    }
}

mxl::native_loot::View view()
{
    return {*d2::level_no,*d2::screen_width,*d2::screen_height,*d2::screen_shift,d2::isPerspective()};
}

glm::ivec2 anchor(d2::UnitAny* unit, bool perspective)
{
    glm::ivec2 point{};
    if (perspective) {
        // Same native projection and item height used by D2Client+0x666D0.
        project(worldX(unit),worldY(unit),96,&point.x,&point.y,0);
    } else {
        // D2Common #10651/#11142 read these two static-path pixel fields.
        point.x=int(unit->v110.pStaticPath->xOffset)-*cameraX+*viewShift;
        point.y=int(unit->v110.pStaticPath->yOffset)-*cameraY+8;
    }
    return point;
}

d2::UnitAny* resolve(const mxl::native_loot::GroundEntry& entry)
{
    return mxl::native_loot::resolve_ground([&](bool second) {
        return (second?d2::findUnitServer:d2::findUnitClient)(entry.id,4);
    },[&](d2::UnitAny* candidate) {
        if(!candidate || candidate->dwType!=d2::UnitType::Item
            || !candidate->v110.pItemData || !candidate->v110.pStaticPath) return false;
        const auto& data=candidate->v110;
        return data.dwUnitId==entry.id && data.dwClassId==entry.base && data.dwInitSeed==entry.seed
            && reinterpret_cast<uintptr_t>(data.pAct)==entry.act && (data.dwMode==3 || data.dwMode==5);
    });
}

mxl::native_loot::Appearance lookFor(d2::UnitAny* unit)
{
    const auto& data=unit->v110;
    const auto& item=*data.pItemData;
    return mxl::native_loot::classify(4,data.dwMode,data.dwClassId,unsigned(item.dwQuality),
        item.dwFlags,playerLevel,item.dwItemLevel);
}

NameEntry& nameFor(d2::UnitAny* unit,const mxl::native_loot::GroundEntry& entry,uint32_t now)
{
    NameEntry* slot=nullptr;
    for(auto& n:names) {
        if(n.used && mxl::native_loot::same_identity(n.item,entry)) { slot=&n;break; }
        if(!slot && !n.used) slot=&n;
    }
    if(!slot) {
        slot=&names[0];
        for(auto& n:names) if(uint32_t(now-n.lastSeen)>uint32_t(now-slot->lastSeen)) slot=&n;
    }
    const auto& data=*unit->v110.pItemData;
    if(!slot->used || !mxl::native_loot::same_identity(slot->item,entry)
        || slot->flags!=data.dwFlags || slot->quality!=unsigned(data.dwQuality)
        || mxl::native_loot::refresh_ground_name(!slot->text[0],now,slot->updated)) {
        // An item may arrive before its name is ready. Retry on the next draw,
        // rather than caching an empty result for the normal five seconds.
        if(slot->used && mxl::native_loot::same_identity(slot->item,entry) && !slot->text[0]) ++emptyNameRetries;
        *slot={};slot->used=true;slot->item=entry;slot->flags=data.dwFlags;slot->quality=unsigned(data.dwQuality);
        // The same native 1.13c item-name formatter used by ground/inventory
        // labels. No selection changes and no names guessed from rarity.
        itemName(unit,slot->text.data(),uint32_t(slot->text.size()-1));
        slot->text.back()=0;slot->updated=now;++nameFormats;
    }
    slot->lastSeen=now;
    return *slot;
}

bool onItemLabel(const mxl::native_loot::GroundEntry& entry,int ax,int ay,int x,int y,uint32_t now)
{
    for(unsigned i=0;i<groundLabelCount;++i)
        if(groundLabels[i].contains(entry,ax,ay,x,y,now)) return true;
    return label.contains(entry,ax,ay,x,y,now);
}

bool carryingItem()
{
    auto* player=d2::getPlayerUnit();
    if(!player || !cursorItem) return true;
    static_assert(offsetof(d2::UnitAny,v110._1)+12*sizeof(DWORD)==0x60);
    // D2Client's own cursor-item calls pass UnitAny+0x60 to D2Common #11017.
    // Let the native getter validate the inventory signature and cursor slot.
    auto* inventory=*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(player)+0x60);
    return cursorItem(inventory)!=nullptr;
}

bool inputAllowed(int x,int y)
{
    if(!pickupActive) return false;
    const bool locked=*selectionLocked!=0;
    const bool coveredByUi=!mxl::native_loot::world_input_point(*d2::screen_shift,*d2::screen_width,*d2::screen_height,x,y);
    const bool menu=d2::isEscMenuOpen() || option::Menu::instance().isVisible();
    const bool alt=*d2::is_alt_clicked!=0;
    const bool combat=(GetKeyState(VK_RBUTTON)&0x8000) || (GetKeyState(VK_SHIFT)&0x8000);
    hoverGate=(GetCurrentThreadId()!=gameThread?1u:0u) | (GetForegroundWindow()!=App.hwnd?2u:0u)
        | (locked?4u:0u) | (coveredByUi?8u:0u) | (menu?16u:0u) | (alt?32u:0u) | (combat?64u:0u)
        | ((*cursorAction!=0 || *d2::cursor_state1==6 || *d2::cursor_state3>=2 || carryingItem())?256u:0u);
    return !hoverGate && mxl::native_loot::fresh_pick_frame(GetTickCount(),pickDrawn,
        App.game.screen==GameScreen::InGame,locked,coveredByUi,menu,alt,combat);
}

void __stdcall updateSelection()
{
    ++selectionCalls;
    const mxl::native_loot::SelectionKey key{frameRevision,inputRevision,*d2::screen_shift,*selectionLocked,
        unsigned(*d2::is_alt_clicked),*cursorAction,
        unsigned(*d2::cursor_state1) | (unsigned(*d2::cursor_state2)<<8) | (unsigned(*d2::cursor_state3)<<16),
        *d2::mouse_x,*d2::mouse_y,carryingItem()};
    if(!selectionCache.changed(key)) {
        // Retain only our already validated hover during repeated identical
        // polls. Re-resolve its identity and native eligibility each time, so
        // a removed/reused unit cannot remain selected between draw frames.
        if(hoveredValid && !key.carryingItem && !*selectionLocked && hovered.view==view() && GetTickCount()-pickDrawn<=120) {
            auto* unit=resolve(hovered);
            if(unit && unit->v110.dwMode==3 && d2::getSelectedUnit()==unit) return;
            hoveredValid=false;
        }
        originalSelection();
        return;
    }
    originalSelection();
    hoveredValid=false;
    if(!inputAllowed(*d2::mouse_x,*d2::mouse_y) || (GetKeyState(VK_LBUTTON)&0x8000)) return;
    ++hoverChecks;
    // Run the same world/UI gate as native selection. Its outputs are world
    // coordinates; the effect hitboxes below use the original screen mouse.
    int worldX=0,worldY=0;
    if(worldMouse(&worldX,&worldY)) return;
    auto* selected=d2::getSelectedUnit();
    if(selected && selected->dwType!=d2::UnitType::Item) return;
    const int x=*d2::mouse_x,y=*d2::mouse_y;
    const auto viewport=view();
    mxl::native_loot::PickChoice choice;
    for(unsigned i=0;i<pickCount;++i) {
        const auto& entry=pickItems[i];
        if(!(entry.view==viewport)) continue;
        auto* unit=resolve(entry);
        if(!unit || unit->v110.dwMode!=3) continue;
        const auto look=lookFor(unit);
        if(!look.rank) continue;
        const auto pos=anchor(unit,viewport.perspective)+glm::ivec2(entry.localX,entry.localY);
        const auto size=mxl::native_loot::effect_size(look.rank,look.style);
        const bool onLabel=onItemLabel(entry,pos.x,pos.y,x,y,GetTickCount());
        if(!onLabel && !mxl::native_loot::effect_hitbox(pos.x,pos.y,size.width,size.height).contains(x,y)) continue;
        // Native selectability is required; never change its flags or bypass
        // restrictions. Zero disables only the original tiny sprite hit-test.
        if(!selectable(unit,0,0,0)) continue;
        choice.offer(int(i),entry.id,x,y,pos.x,pos.y,onLabel);
    }
    if(choice.index<0) return;
    const auto& entry=pickItems[choice.index];
    auto* unit=resolve(entry);
    if(!unit || unit->v110.dwMode!=3 || !selectable(unit,0,0,0)) return;
    selectNative(unit);
    if(d2::getSelectedUnit()==unit) {
        hovered=entry;hoveredValid=true;++hoverSelections;
        if(viewport.panels) ++panelHoverSelections;
    }
}

void emit(d2::UnitAny* unit,int x,int y,const mxl::native_loot::GroundEntry& entry)
{
    const auto& data=unit->v110;
    const auto look=lookFor(unit);
    if (!look.rank || !budget.take(look.rank)) return;
    lastX=x;lastY=y;
    const auto& animation=(*animations)[look.profile];
    const unsigned frame=(tick/50+data.dwUnitId%24)%mxl::native_loot::frame_count;
    auto drawAnimation=[&](d2::CellFile* file,int mode,unsigned imageFrame) {
        const unsigned parts=file->numcells/mxl::native_loot::frame_count;
        for(unsigned part=0;part<parts;++part) {
            const unsigned index=imageFrame*parts+part;
            d2::CellContext cell{};
            cell.v113.nCellNo=index;
            cell.v113.pCellFile=file;
            cell.v113.pCurGfxCell=file->cells[index];
            // Each cell carries its own world offset. Tall beams join exactly
            // at a row boundary and retain the same native world draw order.
            d2::drawImage(&cell,x,y,0xffffffff,mode,nullptr);
            ++spriteDraws;
        }
    };
    const auto age=pulseHistory.age(entry,tick,data.dwMode==3);
    if(age<mxl::native_loot::landing_pulse_ms) {
        auto* pulse=(*landingAnimations)[(look.rank-1)*6+look.colour].file;
        drawAnimation(pulse,3,age/33);
        ++landingDraws;
    }
    if (look.rank>=2) {
        // Native DrawMode 3 -> D2Glide ONE+ONE, including the DX12 path.
        drawAnimation(animation.glow,3,frame);
        ++bloomDraws;
    }
    // Resampled edges contain premultiplied light. Additive drawing lets the
    // world show through them without dark fringes around the soft edges.
    drawAnimation(animation.file,look.rank>=2?3:5,frame);
    ++effectDraws;
    ++profileDraws[look.profile];
    if(pickCount<pickItems.size()) pickItems[pickCount++]=entry;
}

void __stdcall floorEffects()
{
    if (!active || floorPainted || App.game.screen!=GameScreen::InGame
        || App.game.draw_stage!=DrawStage::World) return;
    floorPainted=true; ++floorPasses;
    pickCount=0;pickDrawn=GetTickCount();
    const auto viewport=view();
    for (unsigned i=0;i<queue.visibleCount;++i) {
        const auto& entry=queue.visible[i];
        if (!(entry.view==viewport)) { ++viewRejected;continue; }
        bool secondTable=false;
        auto* unit=mxl::native_loot::resolve_ground([&](bool second) {
            secondTable=second;
            return (second?d2::findUnitServer:d2::findUnitClient)(entry.id,4);
        },[&](d2::UnitAny* candidate) {
            if (!candidate || candidate->dwType!=d2::UnitType::Item
                || !candidate->v110.pItemData || !candidate->v110.pStaticPath) return false;
            const auto& data=candidate->v110;
            const bool matches=data.dwUnitId==entry.id && data.dwClassId==entry.base && data.dwInitSeed==entry.seed
                && reinterpret_cast<uintptr_t>(data.pAct)==entry.act && (data.dwMode==3 || data.dwMode==5);
            if(!matches) ++identityRejected;
            return matches;
        });
        if(!unit) { pulseHistory.forget(entry);++lookupRejected;continue; }
        if(secondTable) ++secondTableMatches;
        const auto pos=anchor(unit,viewport.perspective);
        emit(unit,pos.x+entry.localX,pos.y+entry.localY,entry);
    }
}

bool hashMatches(const std::filesystem::path& path, const char* expected)
{
    HCRYPTPROV provider = 0; HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)) return false;
    bool ok = CryptCreateHash(provider,CALG_SHA_256,0,0,&hash) != FALSE;
    std::ifstream file(path,std::ios::binary);
    if (!file) ok = false;
    char buffer[16384];
    while (ok && file) {
        file.read(buffer,sizeof(buffer));
        if (file.gcount()) ok = CryptHashData(hash,reinterpret_cast<BYTE*>(buffer),DWORD(file.gcount()),0) != FALSE;
    }
    if (file.bad()) ok = false;
    BYTE digest[32]{}; DWORD size = sizeof(digest);
    if (ok) ok = CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0) != FALSE;
    char result[65]{};
    if (ok) for (unsigned i=0;i<32;++i) std::sprintf(result+i*2,"%02x",digest[i]);
    if (hash) CryptDestroyHash(hash);
    CryptReleaseContext(provider,0);
    return ok && std::strcmp(result,expected) == 0;
}

bool loadAnimation(unsigned resource,unsigned cells,std::vector<uint8_t>& bytes,d2::CellFile*& file,Normalize normalize)
{
    const auto start=StartupClock::now();
    const auto info=FindResourceW(App.hmodule,MAKEINTRESOURCEW(resource),RT_RCDATA);
    const auto handle=info?LoadResource(App.hmodule,info):nullptr;
    const auto* data=handle?static_cast<const uint8_t*>(LockResource(handle)):nullptr;
    const auto size=info?SizeofResource(App.hmodule,info):0;
    const bool decoded=data && mxl::native_loot::unpack_asset({data,size},cells,bytes,stbi_zlib_decode_buffer);
    startupUnpackMs+=startupElapsed(start);
    if(!decoded) return false;
    const auto nativeStart=StartupClock::now();
    normalize(bytes.data(),&file,__FILE__,__LINE__,-1,0);
    startupNormalizeMs+=startupElapsed(nativeStart);
    ++startupAssets;
    return file==reinterpret_cast<d2::CellFile*>(bytes.data()) && file->numcells==cells;
}

uint32_t __fastcall worldDraw(d2::UnitAny* unit, uint32_t light, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    ++worldCalls;
    // Enter through actual native world-unit drawing, which Sigma demonstrably
    // uses. Do not depend on the stock caller's optional 0x9F270 render phase.
    // The first unit of the frame emits the captured effects before itself.
    floorEffects();
    auto* previous = current;
    const bool previousPainted = painted;
    current = nullptr; painted = false;
    if (unit && unit->dwType == d2::UnitType::Item && unit->v110.pItemData
        && (unit->v110.dwMode == 3 || unit->v110.dwMode == 5)) {
        current = unit;
        ++groundCalls;
    }
    const auto result = original(unit,light,a,b,c,d);
    current = previous; painted = previousPainted;
    return result;
}

void initialize()
{
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(App.hmodule,path,MAX_PATH)) return;
    directory = std::filesystem::path(path).parent_path();
    const auto ini = directory / L"mxl-native-loot.ini";
    if (!GetPrivateProfileIntW(L"NativeLoot",L"Enabled",0,ini.c_str())) return;
    struct StartupTiming {
        StartupClock::time_point start=StartupClock::now();
        ~StartupTiming() { startupTotalMs=startupElapsed(start);report("native loot startup timing"); }
    } startupTiming;
    if (helpers::getVersion() != Version::V_113c) { report("disabled: unsupported game version"); return; }
    const struct { const wchar_t* name; const char* hash; } expected[] = {
        {L"D2Client.dll","dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906"},
        {L"D2Sigma.dll","ff44257078d994809d6b1a5a3a28657a75b1bb75395b1ee0714361d78355b728"},
        {L"D2CMP.dll","2ee205f484161c5ed854f30aed12565a7ac3e97fd1a838e697aaefe4dc349756"},
        {L"D2Gfx.dll","50284647a28e8839028e8acb021a6ebe48c006e431139969ccd0fe126e6e8811"},
        {L"D2Common.dll","59fa5928522f566f2bf99675571206ad70df889c89d3d07fa87edf5083e06e10"},
    };
    const auto hashStart=StartupClock::now();
    for (const auto& file : expected) {
        const auto module = GetModuleHandleW(file.name);
        if (!module || !GetModuleFileNameW(module,path,MAX_PATH) || !hashMatches(path,file.hash)) {
            report("disabled: game DLL does not match verified build"); return;
        }
    }
    if (!hashMatches(directory/L"medianxl-YmludGJsdHh0.mpq","52f03a58136426a70a4e6d7c26454c1240fcf7731d004060d4ecba744155ae49")) {
        report("disabled: item tables changed"); return;
    }
    startupHashMs=startupElapsed(hashStart);
    const auto client = reinterpret_cast<uint8_t*>(GetModuleHandleW(L"D2Client.dll"));
    const auto cmp = GetModuleHandleW(L"D2CMP.dll");
    auto normalize = reinterpret_cast<Normalize>(GetProcAddress(cmp,MAKEINTRESOURCEA(10006)));
    const uint8_t expectedEntry[] = {0x83,0xec,0x6c,0x56,0x57,0x89,0x54,0x24,0x08,0x8b,0xf1};
    const uint8_t expectedNormalize[] = {0x56,0x8b,0x74,0x24,0x08,0x85,0xf6};
    if (!normalize || std::memcmp(client+0x6cc00,expectedEntry,sizeof(expectedEntry))
        || std::memcmp(reinterpret_cast<void*>(normalize),expectedNormalize,sizeof(expectedNormalize))) {
        report("disabled: native draw/loader entry already modified"); return;
    }
    worldX=reinterpret_cast<UnitCoordinate>(GetProcAddress(GetModuleHandleW(L"D2Common.dll"),MAKEINTRESOURCEA(11080)));
    worldY=reinterpret_cast<UnitCoordinate>(GetProcAddress(GetModuleHandleW(L"D2Common.dll"),MAKEINTRESOURCEA(11168)));
    project=reinterpret_cast<Project>(GetProcAddress(GetModuleHandleW(L"D2Gfx.dll"),MAKEINTRESOURCEA(10002)));
    if (!worldX || !worldY || !project) { report("disabled: native coordinate functions missing"); return; }
    cameraX=reinterpret_cast<int*>(client+0x119960);
    cameraY=reinterpret_cast<int*>(client+0x11995c);
    viewShift=reinterpret_cast<int*>(client+0x11c418);
    animations = new std::array<Animation,mxl::native_loot::ProfileCount>;
    for (unsigned i=1;i<animations->size();++i) {
        const auto& p=mxl::native_loot::profiles[i];
        auto& animation = (*animations)[i];
        for(unsigned j=1;j<i;++j) {
            const auto& previous=mxl::native_loot::profiles[j];
            if(p.rank==previous.rank && p.colour==previous.colour && p.style==previous.style) {
                animation.file=(*animations)[j].file;animation.glow=(*animations)[j].glow;break;
            }
        }
        if(animation.file) continue;
        for(bool bloom:{false,true}) {
            if(bloom && p.rank<2) continue;
            auto& bytes=bloom?animation.glowBytes:animation.bytes;
            auto& file=bloom?animation.glow:animation.file;
            if(!loadAnimation(mxl::native_loot::effect_resource(i,bloom),
                mxl::native_loot::frame_count*mxl::native_loot::cell_parts(p.rank,p.style,bloom),bytes,file,normalize)) {
                report("disabled: embedded native sprite invalid or normalization failed"); return;
            }
        }
    }
    landingAnimations=new std::array<Animation,24>;
    for(unsigned rank=1;rank<=4;++rank) for(unsigned colour=0;colour<6;++colour) {
        auto& animation=(*landingAnimations)[(rank-1)*6+colour];
        if(!loadAnimation(mxl::native_loot::landing_resource(rank,colour),
            mxl::native_loot::frame_count*mxl::native_loot::landing_parts(rank),animation.bytes,animation.file,normalize)) {
            report("disabled: embedded landing pulse invalid or normalization failed");return;
        }
    }
    original = reinterpret_cast<WorldDraw>(client+0x6cc00);
    LONG result = DetourTransactionBegin();
    if (result == NO_ERROR) {
        result = DetourUpdateThread(GetCurrentThread());
        if (result == NO_ERROR) result = DetourAttach(&(PVOID&)original,worldDraw);
        if (result == NO_ERROR) result = DetourTransactionCommit();
        else DetourTransactionAbort();
    }
    active = result == NO_ERROR;
    report(active ? "enabled: bigger native loot, star showers, tall beams and pulses (spectacle-v5)" : "disabled: draw hooks could not attach");
    if(!active) return;
    itemName=reinterpret_cast<ItemName>(client+0x914f0);
    if(!GetPrivateProfileIntW(L"NativeLoot",L"ClickEffects",1,ini.c_str())) return;
    // Check relocated absolute operands as well as opcodes. Refuse interaction
    // if another component has changed any of the verified entry points.
    const uint8_t updateTail[]={0x83,0xec,0x34,0x57,0x33,0xff,0x3b,0xc7};
    const uint8_t setter[]={0x81,0xec,0x00,0x02,0x00,0x00,0x56,0x8b,0xf0};
    const uint8_t mouse[]={0x53,0x56,0x8b,0x35};
    const uint8_t eligibilityTail[]={0x83,0xec,0x28,0x48,0x55};
    if(client[0x51e80]!=0xa1 || *reinterpret_cast<uintptr_t*>(client+0x51e81)!=uintptr_t(client+0x11c2f8)
        || std::memcmp(client+0x51e85,updateTail,sizeof(updateTail))
        || std::memcmp(client+0x51860,setter,sizeof(setter))
        || std::memcmp(client+0x51d10,mouse,sizeof(mouse))
        || *reinterpret_cast<uintptr_t*>(client+0x51d14)!=uintptr_t(client+0x11c414)
        || client[0xa68e0]!=0xa1 || *reinterpret_cast<uintptr_t*>(client+0xa68e1)!=uintptr_t(client+0x11c414)
        || std::memcmp(client+0xa68e5,eligibilityTail,sizeof(eligibilityTail))) {
        report("pickup disabled: selection entry differs from verified 1.13c; effects remain enabled");return;
    }
    cursorItem=reinterpret_cast<CursorItem>(GetProcAddress(GetModuleHandleW(L"D2Common.dll"),MAKEINTRESOURCEA(11017)));
    if(!cursorItem) { report("pickup disabled: native cursor-item getter missing");return; }
    selectable=reinterpret_cast<Selectable>(client+0xa68e0);
    selectAddress=client+0x51860;worldMouseAddress=client+0x51d10;
    selectionLocked=reinterpret_cast<uint32_t*>(client+0x11c2f8);
    cursorAction=reinterpret_cast<uint32_t*>(client+0x113a68);
    originalSelection=reinterpret_cast<SelectionUpdate>(client+0x51e80);
    gameThread=GetCurrentThreadId();
    result=DetourTransactionBegin();
    if(result==NO_ERROR) {
        result=DetourUpdateThread(GetCurrentThread());
        if(result==NO_ERROR) result=DetourAttach(&(PVOID&)originalSelection,updateSelection);
        if(result==NO_ERROR) result=DetourTransactionCommit();else DetourTransactionAbort();
    }
    pickupActive=result==NO_ERROR;
    report(pickupActive?"enabled: native hover names and large effect pickup targets (pickup-inventory-v3)":"pickup disabled: selection hook could not attach");
}
}

void beginFrame()
{
    ++frameRevision;
    labelsPainted=false;
    // Sigma has completed loading/patching by the first world frame.
    const bool inGame = App.game.screen == GameScreen::InGame;
    if (!tried && inGame) {
        tried = true;
        try { initialize(); } catch (...) { report("disabled: sprite initialization failed"); }
    }
    if (!active) return;
    auto* player=inGame?d2::getPlayerUnit():nullptr;
    playerLevel=player?d2::getUnitStat(player,12):0;
    if (wasInGame && !inGame) report("left game");
    if (!inGame) { sampleFrame=0;pulseHistory.clear();pickCount=0;hoveredValid=false;label={};selectionCache={};names={};groundLabelCount=0; }
    else {
        ++sampleFrame;
        // Only three startup samples per game, then no frame-loop disk writes.
        if(sampleFrame==60 || sampleFrame==300 || sampleFrame==900) report("startup draw sample");
    }
    wasInGame = inGame;
    budget = {};
    queue.begin(inGame);
    floorPainted=false;
    tick = GetTickCount();
}

void capture(int x, int y)
{
    if (!active || !current || painted || App.game.screen != GameScreen::InGame
        || App.game.draw_stage != DrawStage::World) return;
    painted = true;
    const auto& unit = current->v110;
    const auto& item = *unit.pItemData;
    const auto appearance = mxl::native_loot::classify(4,unit.dwMode,unit.dwClassId,
        unsigned(item.dwQuality),item.dwFlags,playerLevel,item.dwItemLevel);
    if (!appearance.rank || !unit.pStaticPath) return;
    const auto viewport=view();
    const auto base=anchor(current,viewport.perspective);
    const mxl::native_loot::GroundEntry entry={unit.dwUnitId,unit.dwClassId,unit.dwInitSeed,
        reinterpret_cast<uintptr_t>(unit.pAct),x-base.x,y-base.y,viewport};
    if (!queue.remember(entry,appearance.rank)) ++limited;
    else ++captures;
}

void shutdown()
{
    if (!active) return;
    report("shutdown");
    if (DetourTransactionBegin() == NO_ERROR) {
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)original,worldDraw);
        if(pickupActive) DetourDetach(&(PVOID&)originalSelection,updateSelection);
        DetourTransactionCommit();
    }
    active = false;
    pickupActive=false;
}

void hoverLabel(int left,int top,int right,int bottom)
{
    if(!pickupActive || !hoveredValid || *d2::is_alt_clicked || !(hovered.view==view())) return;
    auto* unit=resolve(hovered);
    if(!unit || unit->v110.dwMode!=3 || d2::getSelectedUnit()!=unit) return;
    const auto pos=anchor(unit,hovered.view.perspective)+glm::ivec2(hovered.localX,hovered.localY);
    label={hovered,{left-pos.x-4,top-pos.y-4,right-pos.x+4,bottom-pos.y+4},GetTickCount(),true};
    ++hoverLabels;
    if(hoverLabels==1) report("first native hover name drawn");
    if(hovered.view.panels && !panelNameReported) {
        panelNameReported=true;report("first native hover name with panel open");
    }
}

void beforeLeftClick(int x,int y)
{
    if(!hoveredValid || !inputAllowed(x,y)) return;
    auto* unit=resolve(hovered);
    if(!unit || unit->v110.dwMode!=3 || d2::getSelectedUnit()!=unit) return;
    const auto look=lookFor(unit);
    const auto pos=anchor(unit,hovered.view.perspective)+glm::ivec2(hovered.localX,hovered.localY);
    const auto size=mxl::native_loot::effect_size(look.rank,look.style);
    if(hovered.view==view() && look.rank && selectable(unit,0,0,0)
        && (mxl::native_loot::effect_hitbox(pos.x,pos.y,size.width,size.height).contains(x,y)
            || onItemLabel(hovered,pos.x,pos.y,x,y,GetTickCount()))) {
        ++pickClicks;
        if(hovered.view.panels) ++panelPickClicks;
        if(pickClicks<=5) report("user clicked native loot target");
        else if(hovered.view.panels && panelPickClicks==1) report("user clicked native loot with panel open");
    }
    else { selectNative(nullptr);hoveredValid=false; }
    // Pass the real mouse message through unchanged. The game owns walking,
    // inventory checks, pickup range, and the resulting normal pickup action.
}

void inputMessage(unsigned message)
{
    if(!pickupActive) return;
    switch(message) {
        case WM_ACTIVATE: case WM_ACTIVATEAPP: case WM_KILLFOCUS: case WM_SETFOCUS:
        case WM_CAPTURECHANGED: case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        case WM_MOUSEWHEEL: ++inputRevision;break;
    }
}

void drawLabels()
{
    if(!active || !itemName || labelsPainted || !floorPainted || App.game.screen!=GameScreen::InGame) return;
    labelsPainted=true;groundLabelCount=0;
    if(d2::isEscMenuOpen() || option::Menu::instance().isVisible()) return;
    const auto viewport=view();const auto now=GetTickCount();
    auto* selected=d2::getSelectedUnit();
    for(unsigned i=0;i<pickCount;++i) {
        const auto& entry=pickItems[i];
        if(!(entry.view==viewport)) continue;
        auto* unit=resolve(entry);
        if(!unit || !mxl::native_loot::ground_label_mode(unit->v110.dwMode)) continue;
        const auto look=lookFor(unit);
        if(!look.rank) continue;
        const auto pos=anchor(unit,viewport.perspective)+glm::ivec2(entry.localX,entry.localY);
        if(!mxl::native_loot::world_input_point(viewport.panels,viewport.width,viewport.height,pos.x,pos.y)) continue;
        const auto& name=nameFor(unit,entry,now);
        glm::ivec4 bounds{};
        constexpr uint32_t colors[]={3,4,2,11,1,0};
        if(!HDText::Instance().drawLootLabel(name.text.data(),pos.x,pos.y,colors[look.colour],look.rank,selected==unit,bounds)) continue;
        groundLabels[groundLabelCount++]={entry,{bounds.x-pos.x-3,bounds.y-pos.y-3,bounds.z-pos.x+3,bounds.w-pos.y+3},now,true};
        ++permanentLabels;
        if(unit->v110.dwMode==5 && ++fallingLabels==1) report("first loot label during drop animation");
        if(permanentLabels==1) report("first permanent compact value-scaled loot label drawn");
    }
}

bool suppressHoverLabel()
{
    if(!active || !itemName || App.game.screen!=GameScreen::InGame || d2::isEscMenuOpen() || option::Menu::instance().isVisible()
        || !mxl::native_loot::world_input_point(*d2::screen_shift,*d2::screen_width,*d2::screen_height,*d2::mouse_x,*d2::mouse_y)) return false;
    auto* selected=d2::getSelectedUnit();
    if(!selected || selected->dwType!=d2::UnitType::Item || selected->v110.dwMode!=3) return false;
    const auto viewport=view();
    for(unsigned i=0;i<groundLabelCount;++i) {
        const auto& entry=groundLabels[i].item;
        if(groundLabels[i].valid && uint32_t(GetTickCount()-groundLabels[i].painted)<=120
            && entry.view==viewport && entry.id==selected->v110.dwUnitId && resolve(entry)==selected) return true;
    }
    return false;
}
}
