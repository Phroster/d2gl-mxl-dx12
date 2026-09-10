// SPDX-License-Identifier: GPL-3.0-or-later
// Private build only. Buffer observations while playing; write on leaving a
// game or normal shutdown. No room traversal and no retained unit pointers.
#if MXL_ENABLE_DIAGNOSTICS
struct ObjectAuditRow {
    uint32_t id=0,base=0,seed=0,level=0,mode=0,subclass=0,operate=0,draw=0,selectable=0,door=0;
    uint32_t rank=0,colour=0,seen=0,captured=0,painted=0,named=0,hovered=0;
    int rawX=0,rawY=0,x=0,y=0,cameraX=0,cameraY=0,shift=0;
    bool used=false;
};
std::array<ObjectAuditRow,1024> objectAuditRows{};
unsigned objectAuditDropped=0,objectAuditSession=0;
uint32_t objectAuditHoverTime=0;
bool objectAuditDirty=false;
ObjectAuditRow* auditRow(uint32_t id,uint32_t base,uint32_t seed,uint32_t level)
{
    for(unsigned n=0;n<objectAuditRows.size();++n) {
        auto& r=objectAuditRows[(id*2654435761u+n)%objectAuditRows.size()];
        if(r.used && (r.id!=id || r.base!=base || r.seed!=seed || r.level!=level)) continue;
        if(!r.used) {r={};r.used=true;r.id=id;r.base=base;r.seed=seed;r.level=level;}
        objectAuditDirty=true;return &r;
    }
    ++objectAuditDropped;return nullptr;
}
void auditObject(d2::UnitAny* unit,int rawX,int rawY,bool hover)
{
    if(!objectIndicatorsEnabled || !unit || unit->dwType!=d2::UnitType::Object) return;
    const auto& data=unit->v110;
    auto* r=auditRow(data.dwUnitId,data.dwClassId,data.dwInitSeed,*d2::level_no);
    if(!r) return;
    if(hover) ++r->hovered;else ++r->seen;
    r->mode=data.dwMode;
    r->cameraX=*cameraX;r->cameraY=*cameraY;r->shift=*viewShift;
    if(!hover) {r->rawX=rawX;r->rawY=rawY;}
    const uint8_t* table=nullptr;
    if(data.pObjectData) std::memcpy(&table,data.pObjectData,sizeof(table));
    if(!table) return;
    r->subclass=table[0x167];r->operate=table[0x1b3];r->draw=table[0x150];r->door=table[0x13a];
    r->selectable=data.dwMode<8?table[0xc4+data.dwMode]:0;
    const auto look=objectLook(unit);r->rank=look.rank;r->colour=look.colour;
}
void auditObjectDraw(const mxl::native_loot::ObjectIndicator& entry,unsigned event)
{
    const auto& id=entry.identity;
    if(auto* r=auditRow(id.id,id.base,id.seed,id.view.level)) {
        r->x=entry.x;r->y=entry.y;
        if(event==0) ++r->captured;else if(event==1) ++r->painted;else ++r->named;
    }
}
void auditObjectHover()
{
    if(!objectIndicatorsEnabled || uint32_t(GetTickCount()-objectAuditHoverTime)<250) return;
    objectAuditHoverTime=GetTickCount();
    auditObject(d2::getSelectedUnit(),0,0,true);
}
void flushObjectAudit()
{
    if(!objectAuditDirty || directory.empty()) return;
    const auto path=directory/("mxl-object-audit-"+std::to_string(GetCurrentProcessId())+".log");
    if(FILE* f=_wfopen(path.c_str(),L"a")) {
        std::fprintf(f,"object_audit_schema=1 session=%u dropped=%u (private build; buffered until leaving game)\n",++objectAuditSession,objectAuditDropped);
        std::fprintf(f,"id,base,seed,level,mode,subclass,operate,draw,selectable,door,rank,colour,seen,captured,painted,named,hovered,raw_x,raw_y,screen_x,screen_y,camera_x,camera_y,shift\n");
        for(const auto& r:objectAuditRows) if(r.used)
            std::fprintf(f,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%d\n",
                r.id,r.base,r.seed,r.level,r.mode,r.subclass,r.operate,r.draw,r.selectable,r.door,r.rank,r.colour,
                r.seen,r.captured,r.painted,r.named,r.hovered,r.rawX,r.rawY,r.x,r.y,r.cameraX,r.cameraY,r.shift);
        std::fclose(f);
    }
    objectAuditRows={};objectAuditDropped=0;objectAuditDirty=false;
}
#else
#define auditObject(...) ((void)0)
#define auditObjectDraw(...) ((void)0)
#define auditObjectHover(...) ((void)0)
#define flushObjectAudit(...) ((void)0)
#endif
