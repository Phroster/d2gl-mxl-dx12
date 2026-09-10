// SPDX-License-Identifier: GPL-3.0-or-later
// Included inside NativeLoot's private namespace after its native draw state.
bool objectIndicatorsEnabled=false;
mxl::native_loot::ObjectIndicators objectIndicators;
std::array<mxl::native_loot::HoverLabel,mxl::native_loot::object_indicator_limit> oldObjectLabels{};
unsigned oldObjectLabelCount=0;

void captureObject(d2::UnitAny* unit,int x,int y)
{
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
    uint32_t sx=0,sy=0;
    std::memcpy(&sx,table+0xd0,4);std::memcpy(&sy,table+0xd4,4);
    const mxl::native_loot::ObjectFacts facts={data.dwMode,table[0x167],table[0x1b3],sx,sy,data.dwClassId,
        table[0xc4+data.dwMode]!=0,table[0x150]!=0,table[0x13a]!=0,(object[4]&0x80)!=0};
    const auto look=mxl::native_loot::object_look(facts);
    if(!look.rank) return;
    const auto viewport=view();
    // a/b in the verified D2Client+6CC00 entry are its projected world origin.
    // Native object art adds its own offsets; labels and rings stay at its feet.
    const auto offset=MotionPrediction::Instance().isActive()?MotionPrediction::Instance().getGlobalOffset():glm::ivec2{};
    x-=offset.x;y-=offset.y;
    if(!mxl::native_loot::world_input_point(viewport.panels,viewport.width,viewport.height,x,y)) return;
    mxl::native_loot::ObjectIndicator entry{};
    entry.identity={data.dwUnitId,data.dwClassId,data.dwInitSeed,reinterpret_cast<uintptr_t>(data.pAct),0,0,viewport};
    entry.x=x;entry.y=y;entry.look=look;
    static_assert(sizeof(wchar_t)==2);
    std::memcpy(entry.name.data(),table+0x40,64*sizeof(wchar_t));
    entry.name.back()=0;
    for(auto& c:entry.name) { if(c==L'\r' || c==L'\n' || c==L'\t') c=L' '; }
    if(!entry.name[0]) {
        const wchar_t* fallback=look.kind==mxl::native_loot::ObjectKind::Shrine?L"Shrine":
            look.kind==mxl::native_loot::ObjectKind::Waypoint?L"Waypoint":
            look.kind==mxl::native_loot::ObjectKind::Well?L"Well":
            look.kind==mxl::native_loot::ObjectKind::Stash?L"Stash":L"Container";
        std::wcsncpy(entry.name.data(),fallback,entry.name.size()-1);
    }
    objectIndicators.remember(entry);
}

void paintObjectEffects()
{
    if(!objectIndicatorsEnabled || !objectIndicators.count || !landingAnimations || !floorPainted
        || App.game.draw_stage!=DrawStage::World || d2::isEscMenuOpen() || option::Menu::instance().isVisible()) return;
    const auto viewport=view();
    for(unsigned i=0;i<objectIndicators.count;++i) {
        const auto& entry=objectIndicators.entries[i];
        if(!(entry.identity.view==viewport) || !entry.look.pulseRank) continue;
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
