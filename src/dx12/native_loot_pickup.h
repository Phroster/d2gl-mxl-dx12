// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include "native_loot_layer.h"
namespace mxl::native_loot {
// A readable floor for quiet supplies, then larger labels for valuable,
// rare and exceptional loot in the same catalog used by the effects.
constexpr float ground_label_scale(unsigned rank) {
    constexpr float scales[]={.95f,.95f,1.10f,1.25f,1.40f};
    return scales[std::min(rank,4u)];
}
// Names follow visible loot effects during the drop animation (5), without
// waiting for resting ground mode (3). Pickup still uses native ground gates.
constexpr bool ground_label_mode(uint32_t mode) { return mode==3 || mode==5; }
constexpr bool refresh_ground_name(bool empty,uint32_t now,uint32_t updated) {
    return empty || uint32_t(now-updated)>5000;
}
struct HitRect {
    int left=0,top=0,right=0,bottom=0;
    bool contains(int x,int y) const {
        return left<right && top<bottom && x>=left && x<=right && y>=top && y<=bottom;
    }
};
// Match the native 1.13c world-mouse gate: inventory on the right leaves
// the left half interactive; a left panel leaves the right half. The HUD,
// covered half and a view with both panels open belong to the normal UI.
inline HitRect world_input_rect(uint32_t panels,int width,int height) {
    if(width<=0 || height<=49 || panels>2) return {};
    return {panels==2?width/2:0,0,panels==1?width/2-1:width-1,height-49};
}
inline bool world_input_point(uint32_t panels,int width,int height,int x,int y) {
    return world_input_rect(panels,width,height).contains(x,y);
}
// Screen coordinates are native game pixels, before window/display scaling.
// Keep the floor glow easy to click; include the entire bright beam column.
inline HitRect effect_hitbox(int x,int y,int width,int height) {
    const int half=std::max(44,width/2+8);
    return {x-half,y-std::max(48,height-12),x+half,y+24};
}
inline bool same_ground(const GroundEntry& a,const GroundEntry& b) {
    return a.id==b.id && a.base==b.base && a.seed==b.seed && a.act==b.act && a.view==b.view;
}
inline bool same_identity(const GroundEntry& a,const GroundEntry& b) {
    return a.id==b.id && a.base==b.base && a.seed==b.seed && a.act==b.act;
}
struct HoverLabel {
    GroundEntry item{};
    HitRect relative{};
    uint32_t painted=0;
    bool valid=false;
    bool contains(const GroundEntry& target,int anchorX,int anchorY,int x,int y,uint32_t now) const {
        return valid && uint32_t(now-painted)<=120 && same_ground(item,target)
            && relative.contains(x-anchorX,y-anchorY);
    }
};
struct PickChoice {
    int index=-1;
    int64_t distance=INT64_MAX;
    uint32_t id=UINT32_MAX;
    bool label=false;
    void offer(int candidate,uint32_t itemId,int x,int y,int anchorX,int anchorY,bool onLabel) {
        const int64_t dx=int64_t(x)-anchorX,dy=int64_t(y)-anchorY;
        const int64_t score=dx*dx+dy*dy;
        if(index<0 || (onLabel && !label) || (onLabel==label &&
            (score<distance || (score==distance && itemId<id)))) {
            index=candidate;distance=score;id=itemId;label=onLabel;
        }
    }
};
// The uncapped native input loop may poll thousands of times between draws.
// Reconsider the extended target when its view, mouse, or input state changes.
struct SelectionKey {
    uint32_t frame=0,input=0,panels=0,locked=0,alt=0,cursorAction=0,cursor=0;
    int x=0,y=0;
    bool carryingItem=false;
    bool operator==(const SelectionKey&) const = default;
};
struct SelectionCache {
    SelectionKey key{};
    bool valid=false;
    bool changed(const SelectionKey& next) {
        if(valid && key==next) return false;
        key=next;valid=true;return true;
    }
};
inline bool fresh_pick_frame(uint32_t now,uint32_t drawn,bool inGame,bool locked,bool coveredByUi,bool menu,bool alt,bool combatInput) {
    return inGame && !locked && !coveredByUi && !menu && !alt && !combatInput && uint32_t(now-drawn)<=120;
}
}
