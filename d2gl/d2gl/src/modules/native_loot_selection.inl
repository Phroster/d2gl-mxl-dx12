// SPDX-License-Identifier: GPL-3.0-or-later
// Shared by the live hook and its isolated native-selection regression test.
void __stdcall updateSelection()
{
    mxl::diag::ProducerSampleScope timing(mxl::diag::Metric::LootPickupSampled,
        mxl::diag::Count::LootSelectionCalls,mxl::diag::Count::LootSelectionSamples);
    ++selectionCalls;
    // The native input loop can poll thousands of times between draw frames.
    // Without an eligible target, do not query inventory, keys or unit state.
    // Invalidate the custom hover/cache so new loot is checked immediately.
    if(!pickCount && !objectTargetsAvailable()) {
        hoveredValid=false;
        hoveredObjectValid=false;
        selectionCache={};
        originalSelection();
        return;
    }
    const mxl::native_loot::SelectionKey key{frameRevision,inputRevision,*d2::screen_shift,*selectionLocked,
        unsigned(*d2::is_alt_clicked),*cursorAction,
        unsigned(*d2::cursor_state1) | (unsigned(*d2::cursor_state2)<<8) | (unsigned(*d2::cursor_state3)<<16),
        *d2::mouse_x,*d2::mouse_y};
    if(!selectionCache.changed(key)) {
        // A missed hover cannot acquire a custom target until new input or a
        // new draw frame. Avoid querying inventory on every identical poll.
        // Retain only our already validated hover during repeated identical
        // polls. A retained target still needs a fresh held-item check and
        // identity lookup, even if inventory changed without a window event.
        if(hoveredValid && !*selectionLocked && hovered.view==view() && GetTickCount()-pickDrawn<=120 && !carryingItem()) {
            auto* unit=resolve(hovered);
            if(unit && unit->v110.dwMode==3 && d2::getSelectedUnit()==unit) return;
        }
        if(hoveredObjectValid && inputAllowed(*d2::mouse_x,*d2::mouse_y) && retainObjectHover()) return;
        hoveredValid=false;
        hoveredObjectValid=false;
        originalSelection();
        return;
    }
    originalSelection();
    hoveredValid=false;
    hoveredObjectValid=false;
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
    if(choice.index<0) {
        // Keep native object/monster selection and actual loot priority. The
        // extension only fills empty ground around a visible cue or name.
        if(!selected) selectObjectAt(x,y);
        return;
    }
    const auto& entry=pickItems[choice.index];
    auto* unit=resolve(entry);
    if(!unit || unit->v110.dwMode!=3 || !selectable(unit,0,0,0)) return;
    selectNative(unit);
    if(d2::getSelectedUnit()==unit) {
        hovered=entry;hoveredValid=true;++hoverSelections;
        if(viewport.panels) ++panelHoverSelections;
    }
}
