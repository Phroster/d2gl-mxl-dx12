// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include "native_loot_layer.h"
#include "native_loot_pickup.h"

namespace mxl::native_loot {
constexpr unsigned object_indicator_limit=1024;
constexpr unsigned object_label_limit=12;
struct ObjectScreenPoint { int x=0,y=0; };
// D2Client+6C490 receives world-pixel coordinates. Its flat projection at
// +6C4EC subtracts the camera, adds the horizontal view shift and an 8px Y bias.
constexpr ObjectScreenPoint object_screen_point(int worldX,int worldY,int cameraX,int cameraY,int viewShift) {
    return {worldX-cameraX+viewShift,worldY-cameraY+8};
}
enum class ObjectKind : uint8_t { None,Container,Shrine,Special,Well,Waypoint,Stash,Usable };
struct ObjectFacts {
    unsigned mode=0,subclass=0,operate=0,sizeX=0,sizeY=0,classId=0;
    bool selectable=false,drawn=false,door=false,locked=false;
};
struct ObjectLook {
    ObjectKind kind=ObjectKind::None;
    unsigned rank=0,colour=0,pulseRank=0,priority=0;
};
inline ObjectLook object_look(const ObjectFacts& f) {
    if(!f.drawn || !f.selectable || f.mode>=8 || f.door || (f.subclass&0x80)) return {};
    // Reusable objects remain useful after activation; consumed containers
    // and shrines lose their cue when they leave neutral.
    if((f.subclass&0x40) || f.operate==23) return {ObjectKind::Waypoint,1,3,0,8};
    if(f.operate==32) return {ObjectKind::Stash,1,0,0,7};
    if((f.subclass&1) || f.operate==2) return f.mode==0?ObjectLook{ObjectKind::Shrine,2,3,1,24}:ObjectLook{};
    if((f.subclass&0x20) || f.operate==22) return {ObjectKind::Well,1,3,1,6};
    // Breakable/openable supplies keep only the lowest-priority pulse.
    // Exploding variants use a red pulse rather than a treasure colour.
    if(f.operate==3 || f.operate==5) return f.mode==0?ObjectLook{ObjectKind::Container,1,5,1,4}:ObjectLook{};
    if(f.operate==7 || f.operate==30 || f.operate==68) return f.mode==0?ObjectLook{ObjectKind::Container,1,4,1,3}:ObjectLook{};
    switch(f.operate) {
        case 21:case 28:case 33:case 39:case 40:case 41:
        case 49:case 57:case 58:case 59:
            return f.mode==0?ObjectLook{ObjectKind::Special,2,1,2,28}:ObjectLook{};
    }
    const bool container=(f.subclass&8) || f.operate==1 || f.operate==4 || f.operate==14
        || f.operate==19 || f.operate==20 || f.operate==26 || f.operate==48 || f.operate==51;
    // The native per-mode selectable flag is authoritative for other usable
    // objects, including mod-defined interactables. Unclassified does not mean
    // invisible: give them a quiet cue instead of silently skipping them.
    if(!container) return {ObjectKind::Usable,1,5,0,1};
    if(f.mode!=0) return {};
    // The native special chest ID and locked state are observable properties,
    // not a prediction of the hidden contents or their eventual rarity.
    if(f.classId==397) return {ObjectKind::Special,2,1,2,27};
    if(f.locked) return {ObjectKind::Container,2,1,1,20};
    if(f.sizeX>=3 && f.sizeY>=3) return {ObjectKind::Container,2,5,1,16};
    return {ObjectKind::Container,1,5,1,12};
}
inline bool object_has_label(const ObjectLook& look) {
    return look.rank>=2 || look.kind==ObjectKind::Waypoint || look.kind==ObjectKind::Stash;
}
struct ObjectIndicator {
    GroundEntry identity{};
    int x=0,y=0;
    ObjectLook look{};
    std::array<wchar_t,65> name{};
};
inline HitRect object_hitbox(const ObjectIndicator& e) {
    const int half=e.look.rank>=2?40:14;
    return {e.x-half,e.y-(e.look.rank>=2?32:20),e.x+half,e.y+12};
}
// Current-frame snapshots only: no room scans, no retained game pointers,
// no lookups when the screen has no eligible objects, and a fixed draw limit.
struct ObjectIndicators {
    std::array<ObjectIndicator,object_indicator_limit> entries{};
    unsigned count=0;
    void clear() { count=0; }
    static bool before(const ObjectIndicator& a,const ObjectIndicator& b) {
        if(a.look.priority!=b.look.priority) return a.look.priority>b.look.priority;
        auto distance=[](const auto& e) {
            const int64_t x=int64_t(e.x)-e.identity.view.width/2,y=int64_t(e.y)-e.identity.view.height/2;
            return x*x+y*y;
        };
        const auto da=distance(a),db=distance(b);
        return da!=db?da<db:a.identity.id<b.identity.id;
    }
    void remember(const ObjectIndicator& e) {
        if(!e.look.rank || !e.name[0]) return;
        for(unsigned i=0;i<count;++i) if(entries[i].identity.id==e.identity.id) { entries[i]=e;return; }
        if(count<entries.size()) { entries[count++]=e;return; }
        unsigned worst=0;
        for(unsigned i=1;i<count;++i) if(before(entries[worst],entries[i])) worst=i;
        if(before(e,entries[worst])) entries[worst]=e;
    }
    struct Labels { std::array<unsigned,object_label_limit> indices{};unsigned count=0; };
    Labels labels() const {
        Labels result;
        for(unsigned i=0;i<count;++i) if(object_has_label(entries[i].look)) {
            unsigned at=0;
            while(at<result.count && !before(entries[i],entries[result.indices[at]])) ++at;
            if(at==object_label_limit) continue;
            const unsigned end=std::min(result.count,object_label_limit-1);
            for(unsigned j=end;j>at;--j) result.indices[j]=result.indices[j-1];
            result.indices[at]=i;result.count=std::min(result.count+1,object_label_limit);
        }
        return result;
    }
};
inline unsigned object_pulse_frame(uint32_t tick,uint32_t id) {
    // Ease back and forth through a soft part of the existing pulse. Avoid
    // repeatedly playing the bright landing flash or jumping at loop wrap.
    const unsigned phase=(tick/110+id%24)%24;
    return 9+(phase<12?phase:23-phase);
}
}
