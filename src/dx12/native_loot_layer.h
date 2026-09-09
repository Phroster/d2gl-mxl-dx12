// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include "native_loot_rules.h"
namespace mxl::native_loot {
struct View {
    int level=0;
    uint32_t width=0,height=0,panels=0;
    bool perspective=false;
    bool operator==(const View&) const = default;
};
struct GroundEntry {
    uint32_t id=0,base=0,seed=0;
    uintptr_t act=0;
    int localX=0,localY=0;
    View view{};
};
constexpr uint32_t landing_pulse_ms=792;
// Small fixed history retains identities across camera/panel changes. No unit
// pointers are stored. A flight -> ground transition can trigger a new pulse
// for a player re-dropping the same item; merely seeing it again cannot.
struct PulseHistory {
    struct Entry {
        uint32_t id=0,base=0,seed=0,started=0,lastSeen=0;
        uintptr_t act=0;
        bool used=false,grounded=false;
        bool matches(const GroundEntry& e) const { return used&&id==e.id&&base==e.base&&seed==e.seed&&act==e.act; }
    };
    std::array<Entry,512> items{};
    uint32_t age(const GroundEntry& item,uint32_t now,bool grounded) {
        Entry* slot=nullptr;
        for(auto& e:items) {
            if(e.matches(item)) { slot=&e;break; }
            if(!slot && !e.used) slot=&e;
        }
        if(!slot) {
            // Evict the least recently seen identity, never a newly tracked
            // item in the same frame. The visible draw budget is only 60.
            slot=&items[0];
            for(auto& e:items) if(uint32_t(now-e.lastSeen)>uint32_t(now-slot->lastSeen)) slot=&e;
        }
        if(!slot->matches(item)) *slot={item.id,item.base,item.seed,now,now,item.act,true,false};
        if(grounded && !slot->grounded) slot->started=now;
        slot->grounded=grounded;slot->lastSeen=now;
        return grounded?uint32_t(now-slot->started):landing_pulse_ms;
    }
    void forget(const GroundEntry& item) { for(auto& e:items) if(e.matches(item)) { e={};break; } }
    void clear() { items={}; }
};
// Both tables are in this client process. Ordinary replicated ground items
// can live in the second table, while visual/client-owned units use the first.
// Validate the complete identity before accepting either candidate.
template<class Lookup,class Matches>
auto resolve_ground(Lookup lookup,Matches matches) {
    auto* unit=lookup(false);
    if(matches(unit)) return unit;
    unit=lookup(true);
    return matches(unit)?unit:nullptr;
}
// Only identities actually drawn last frame are eligible. Resolve them again
// through the client's unit table; never retain a unit pointer or old screen XY.
struct GroundQueue {
    std::array<GroundEntry,60> captured{}, visible{};
    unsigned capturedCount=0, visibleCount=0;
    Budget budget{};
    void begin(bool inGame) {
        visibleCount=inGame?capturedCount:0;
        for(unsigned i=0;i<visibleCount;++i) visible[i]=captured[i];
        capturedCount=0; budget={};
    }
    bool remember(const GroundEntry& entry,unsigned rank) {
        for(unsigned i=0;i<capturedCount;++i) if(captured[i].id==entry.id) return true;
        if(capturedCount==captured.size() || !budget.take(rank)) return false;
        captured[capturedCount++]=entry;
        return true;
    }
};
}
