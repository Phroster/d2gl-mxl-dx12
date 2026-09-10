// SPDX-License-Identifier: GPL-3.0-or-later
// Included inside NativeLoot's private namespace after its native draw state.
bool objectIndicatorsEnabled=false;
mxl::native_loot::ObjectIndicators objectIndicators;
mxl::native_loot::ObjectGroups objectGroups;
mxl::native_loot::ObjectNameCoverage objectNameCoverage;
std::array<mxl::native_loot::HoverLabel,mxl::native_loot::object_label_limit> oldObjectLabels{};
unsigned oldObjectLabelCount=0;
bool objectsPainted=false,hoveredObjectValid=false;
mxl::native_loot::GroundEntry hoveredObject{};
thread_local bool drawingObject=false;
thread_local mxl::native_loot::ObjectSpriteAnchor objectSpriteAnchor;

mxl::native_loot::ObjectLook objectLook(d2::UnitAny* unit)
{
    if(!unit || unit->dwType!=d2::UnitType::Object) return {};
    const auto& data=unit->v110;
    if(!data.pObjectData || !data.pStaticPath || data.dwMode>=8) return {};
    const auto* object=reinterpret_cast<const uint8_t*>(data.pObjectData);
    const uint8_t* table=nullptr;std::memcpy(&table,object,sizeof(table));
    if(!table) return {};
    uint32_t sx=0,sy=0;
    std::memcpy(&sx,table+0xd0,4);std::memcpy(&sy,table+0xd4,4);
    return mxl::native_loot::object_look({data.dwMode,table[0x167],table[0x1b3],sx,sy,data.dwClassId,
        table[0xc4+data.dwMode]!=0,table[0x150]!=0,table[0x13a]!=0,(object[4]&0x80)!=0});
}

d2::UnitAny* resolveObject(const mxl::native_loot::GroundEntry& entry)
{
    return mxl::native_loot::resolve_ground([&](bool second) {
        return (second?d2::findUnitServer:d2::findUnitClient)(entry.id,2);
    },[&](d2::UnitAny* unit) {
        if(!unit || unit->dwType!=d2::UnitType::Object) return false;
        const auto& data=unit->v110;
        return data.dwUnitId==entry.id && data.dwClassId==entry.base && data.dwInitSeed==entry.seed
            && reinterpret_cast<uintptr_t>(data.pAct)==entry.act && objectLook(unit).rank
            && selectable(unit,0,0,0);
    });
}

#include "native_object_audit.inl"

void captureObject(d2::UnitAny* unit,int x,int y)
{
    auditObject(unit,x,y,false);
    if(!objectIndicatorsEnabled || !unit || unit->dwType!=d2::UnitType::Object
        || App.game.draw_stage!=DrawStage::World || App.game.screen!=GameScreen::InGame) return;
    const auto& data=unit->v110;
    if(!data.pObjectData || !data.pStaticPath || data.dwMode>=8) return;
    // D2ObjectData+0 points to the native 0x1c0-byte ObjectsTxt record. These
    // offsets are shared by the verified D2Client's object draw/selection code.
    // initialize() checks the exact game DLLs before this path can run.
    const auto* object=reinterpret_cast<const uint8_t*>(data.pObjectData);
    const uint8_t* table=nullptr;
    std::memcpy(&table,object,sizeof(table));
    if(!table) return;
    const auto look=objectLook(unit);
    if(!look.rank) return;
    const auto viewport=view();
    // a/b at D2Client+6CC00 are WORLD pixels, not screen pixels. The native
    // object branch enters +6C490, which subtracts the camera at +6C4EC.
    // Project before viewport rejection; distant levels otherwise lose every
    // object marker despite their containers being visible on the screen.
    const bool nativeSprite=objectSpriteAnchor.width>0;
    if(nativeSprite) {
        x=objectSpriteAnchor.x;y=objectSpriteAnchor.y;
    } else if(viewport.perspective) {
        const auto projected=anchor(unit,true);x=projected.x;y=projected.y;
    } else {
        const auto projected=mxl::native_loot::object_screen_point(x,y,*cameraX,*cameraY,*viewShift);
        x=projected.x;y=projected.y;
    }
    const auto offset=MotionPrediction::Instance().isActive()?MotionPrediction::Instance().getGlobalOffset():glm::ivec2{};
    // The observed sprite has already passed the native motion adjustment.
    if(!nativeSprite) { x-=offset.x;y-=offset.y; }
    if(!mxl::native_loot::world_input_point(viewport.panels,viewport.width,viewport.height,x,y)) return;
    mxl::native_loot::ObjectIndicator entry{};
    entry.identity={data.dwUnitId,data.dwClassId,data.dwInitSeed,reinterpret_cast<uintptr_t>(data.pAct),0,0,viewport};
    entry.x=x;entry.y=y;entry.look=look;
    static_assert(sizeof(wchar_t)==2);
    std::memcpy(entry.name.data(),table+0x40,64*sizeof(wchar_t));
    entry.name.back()=0;
    for(auto& c:entry.name) { if(c==L'\r' || c==L'\n' || c==L'\t') c=L' '; }
    if(mxl::native_loot::object_placeholder_name(entry.name.data())) {
        const wchar_t* fallback=mxl::native_loot::object_fallback_name(look.kind);
        std::wcsncpy(entry.name.data(),fallback,entry.name.size()-1);
    }
    objectIndicators.remember(entry);
    auditObjectDraw(entry,0);
}

void paintObjectEffects()
{
    if(!objectIndicatorsEnabled || !objectIndicators.count || !landingAnimations || !floorPainted
        || App.game.draw_stage!=DrawStage::World || d2::isEscMenuOpen() || option::Menu::instance().isVisible()) return;
    const auto viewport=view();
    objectGroups.rebuild(objectIndicators);
    objectsPainted=true;
    for(unsigned i=0;i<objectGroups.markers.count;++i) {
        const auto& entry=objectGroups.markers.entries[i];
        if(!(entry.identity.view==viewport)) continue;
        auditObjectDraw(entry,1);
        if(!mxl::native_loot::object_uses_pulse(entry.look)) {
            // Unclassified usable objects keep a minimal cue. Loot containers
            // use their native pulse even at rank 1: the tiny line-only glint
            // was insufficient on the Arcane chest captured by the audit.
            constexpr uint8_t colours[]={158,111,133,155,98,255};
            const uint8_t alpha=uint8_t(144+(mxl::native_loot::object_pulse_frame(tick,entry.identity.id)-9)*4);
            const uint8_t colour=colours[std::min(entry.look.colour,5u)];
            const int x=entry.x,y=entry.y-6;
            d2::drawLine(x-5,y,x,y-3,colour,alpha);
            d2::drawLine(x,y-3,x+5,y,colour,alpha);
            d2::drawLine(x+5,y,x,y+3,colour,alpha);
            d2::drawLine(x,y+3,x-5,y,colour,alpha);
            continue;
        }
        auto* file=(*landingAnimations)[(entry.look.pulseRank-1)*6+entry.look.colour].file;
        const unsigned parts=file->numcells/mxl::native_loot::frame_count;
        const unsigned frame=mxl::native_loot::object_pulse_frame(tick,entry.identity.id);
        for(unsigned part=0;part<parts;++part) {
            const unsigned index=frame*parts+part;
            d2::CellContext cell{};
            cell.v113.nCellNo=index;cell.v113.pCellFile=file;cell.v113.pCurGfxCell=file->cells[index];
            const auto* image=cell.v113.pCurGfxCell;
            const mxl::native_loot::SpriteScope sprite({reinterpret_cast<uintptr_t>(image),
                &image->cols,image->length,image->width,image->height});
            d2::drawImage(&cell,entry.x,entry.y,0xffffffff,3,nullptr);
        }
    }
}
