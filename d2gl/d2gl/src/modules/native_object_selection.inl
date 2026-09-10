// SPDX-License-Identifier: GPL-3.0-or-later
// Shared with the controlled native-selection fixture. Only actual draws
// become targets; no room scans, synthetic clicks or direct operate calls.
bool suppressObjectHoverLabel(d2::UnitAny* selected)
{
    if(!objectIndicatorsEnabled || !oldObjectLabelCount
        || !selected || selected->dwType!=d2::UnitType::Object) return false;
    const auto viewport=view();const auto now=GetTickCount();
    // Suppress only a duplicate of a name we actually painted. Unlabelled
    // objects, names outside the draw budget and expired targets keep theirs.
    for(unsigned i=0;i<objectNameCoverage.count;++i) {
        const auto& member=objectNameCoverage.members[i];
        if(member.label>=oldObjectLabelCount || member.identity.id!=selected->v110.dwUnitId
            || !(member.identity.view==viewport)) continue;
        const auto& name=oldObjectLabels[member.label];
        if(name.valid && uint32_t(now-name.painted)<=120
            && mxl::native_loot::same_ground(name.item,objectNameCoverage.leaders[member.label])
            && resolveObject(member.identity)==selected) return true;
    }
    return false;
}

bool objectTargetsAvailable()
{
    return objectIndicatorsEnabled && objectsPainted && objectIndicators.count;
}
const mxl::native_loot::ObjectIndicator* objectTarget(const mxl::native_loot::GroundEntry& id)
{
    if(!objectTargetsAvailable() || !(id.view==view())) return nullptr;
    for(unsigned i=0;i<objectIndicators.count;++i)
        if(mxl::native_loot::same_ground(objectIndicators.entries[i].identity,id)) return &objectIndicators.entries[i];
    return nullptr;
}
bool onObjectLabel(const mxl::native_loot::ObjectIndicator& entry,int x,int y,uint32_t now)
{
    const auto* marker=objectGroups.marker(entry);
    if(!marker || !mxl::native_loot::same_ground(marker->identity,entry.identity)) return false;
    for(unsigned i=0;i<oldObjectLabelCount;++i)
        if(oldObjectLabels[i].contains(marker->identity,marker->x,marker->y,x,y,now)) return true;
    return false;
}
bool onObjectGlow(const mxl::native_loot::ObjectIndicator& entry,int x,int y)
{
    const auto* marker=objectGroups.marker(entry);
    return mxl::native_loot::object_hitbox(marker && marker->members>1?*marker:entry).contains(x,y);
}
bool objectContains(const mxl::native_loot::ObjectIndicator& entry,int x,int y,uint32_t now)
{
    return onObjectLabel(entry,x,y,now) || onObjectGlow(entry,x,y);
}
bool retainObjectHover()
{
    if(!hoveredObjectValid || !objectTarget(hoveredObject)) return false;
    auto* unit=resolveObject(hoveredObject);
    return unit && d2::getSelectedUnit()==unit;
}
void selectObjectAt(int x,int y,bool groupLabelOnly=false)
{
    if(!objectTargetsAvailable()) return;
    const auto viewport=view();const auto now=GetTickCount();
    mxl::native_loot::PickChoice choice;
    for(unsigned i=0;i<objectIndicators.count;++i) {
        const auto& e=objectIndicators.entries[i];
        if(!(e.identity.view==viewport)) continue;
        const bool label=onObjectLabel(e,x,y,now);
        if(groupLabelOnly) {
            const auto* marker=objectGroups.marker(e);
            if(!label || !marker || marker->members<2) continue;
        } else if(!label && !onObjectGlow(e,x,y)) continue;
        if(!resolveObject(e.identity)) continue;
        choice.offer(int(i),e.identity.id,x,y,e.x,e.y,label);
    }
    if(choice.index<0) return;
    const auto& entry=objectIndicators.entries[choice.index];
    auto* unit=resolveObject(entry.identity);
    if(!unit) return;
    selectNative(unit);
    if(d2::getSelectedUnit()==unit) { hoveredObject=entry.identity;hoveredObjectValid=true; }
}
void beforeObjectClick(int x,int y)
{
    if(!hoveredObjectValid) return;
    const auto* entry=objectTarget(hoveredObject);
    auto* unit=entry?resolveObject(hoveredObject):nullptr;
    if(inputAllowed(x,y) && entry && unit && d2::getSelectedUnit()==unit
        && objectContains(*entry,x,y,GetTickCount())) return;
    // Release only our own target; keep unrelated native selection intact.
    auto* selected=d2::getSelectedUnit();
    if(selected && selected->dwType==d2::UnitType::Object && selected->v110.dwUnitId==hoveredObject.id)
        selectNative(nullptr);
    hoveredObjectValid=false;
}
