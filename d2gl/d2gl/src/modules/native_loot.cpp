// SPDX-License-Identifier: GPL-3.0-or-later
#include "pch.h"
#include "native_loot.h"
#include "d2/common.h"
#include "helpers.h"
#include "native_loot_cells.h"
#include "native_loot_rules.h"
#include "native_loot_layer.h"
#include <detours/detours.h>
#include <wincrypt.h>
#include <cstdio>

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

void report(const char* status)
{
    if (directory.empty()) return;
    const auto path = directory / ("mxl-native-loot-" + std::to_string(GetCurrentProcessId()) + ".log");
    if (FILE* f = _wfopen(path.c_str(), L"a")) {
        std::fprintf(f,"%s ground_calls=%llu effect_draws=%llu limited=%llu floor_passes=%llu bloom_draws=%llu\n",status,groundCalls,effectDraws,limited,floorPasses,bloomDraws);
        std::fprintf(f,"  world_calls=%llu captures=%llu queued=%u visible=%u view_rejected=%llu lookup_rejected=%llu identity_rejected=%llu second_table_matches=%llu last_xy=%d,%d\n",
            worldCalls,captures,queue.capturedCount,queue.visibleCount,viewRejected,lookupRejected,identityRejected,secondTableMatches,lastX,lastY);
        std::fprintf(f,"  character_level=%u native_sprite_draws=%llu landing_pulse_draws=%llu\n",playerLevel,spriteDraws,landingDraws);
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

void emit(d2::UnitAny* unit,int x,int y,const mxl::native_loot::GroundEntry& entry)
{
    const auto& data=unit->v110;
    const auto& item=*data.pItemData;
    const auto look=mxl::native_loot::classify(4,data.dwMode,data.dwClassId,unsigned(item.dwQuality),item.dwFlags,playerLevel);
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
}

void __stdcall floorEffects()
{
    if (!active || floorPainted || App.game.screen!=GameScreen::InGame
        || App.game.draw_stage!=DrawStage::World) return;
    floorPainted=true; ++floorPasses;
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
    if (helpers::getVersion() != Version::V_113c) { report("disabled: unsupported game version"); return; }
    const struct { const wchar_t* name; const char* hash; } expected[] = {
        {L"D2Client.dll","dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906"},
        {L"D2Sigma.dll","ff44257078d994809d6b1a5a3a28657a75b1bb75395b1ee0714361d78355b728"},
        {L"D2CMP.dll","2ee205f484161c5ed854f30aed12565a7ac3e97fd1a838e697aaefe4dc349756"},
        {L"D2Gfx.dll","50284647a28e8839028e8acb021a6ebe48c006e431139969ccd0fe126e6e8811"},
        {L"D2Common.dll","59fa5928522f566f2bf99675571206ad70df889c89d3d07fa87edf5083e06e10"},
    };
    for (const auto& file : expected) {
        const auto module = GetModuleHandleW(file.name);
        if (!module || !GetModuleFileNameW(module,path,MAX_PATH) || !hashMatches(path,file.hash)) {
            report("disabled: game DLL does not match verified build"); return;
        }
    }
    if (!hashMatches(directory/L"medianxl-YmludGJsdHh0.mpq","52f03a58136426a70a4e6d7c26454c1240fcf7731d004060d4ecba744155ae49")) {
        report("disabled: item tables changed"); return;
    }
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
            bytes=mxl::native_loot::make_cells(p.rank,p.colour,bloom,p.style);
            normalize(bytes.data(),&file,__FILE__,__LINE__,-1,0);
            if(file!=reinterpret_cast<d2::CellFile*>(bytes.data())
                || file->numcells!=mxl::native_loot::frame_count*mxl::native_loot::cell_parts(p.rank,p.style,bloom)) {
                report("disabled: native sprite normalization failed"); return;
            }
        }
    }
    landingAnimations=new std::array<Animation,24>;
    for(unsigned rank=1;rank<=4;++rank) for(unsigned colour=0;colour<6;++colour) {
        auto& animation=(*landingAnimations)[(rank-1)*6+colour];
        animation.bytes=mxl::native_loot::make_landing_cells(rank,colour);
        normalize(animation.bytes.data(),&animation.file,__FILE__,__LINE__,-1,0);
        if(animation.file!=reinterpret_cast<d2::CellFile*>(animation.bytes.data())
            || animation.file->numcells!=mxl::native_loot::frame_count) {
            report("disabled: landing pulse normalization failed");return;
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
}
}

void beginFrame()
{
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
    if (!inGame) { sampleFrame=0;pulseHistory.clear(); }
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
        unsigned(item.dwQuality),item.dwFlags,playerLevel);
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
        DetourTransactionCommit();
    }
    active = false;
}
}
