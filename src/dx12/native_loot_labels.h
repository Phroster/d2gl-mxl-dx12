// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cstdlib>
#include <climits>
#include <span>
#include "native_loot_pickup.h"

namespace mxl::native_loot {
// Leave room for the three-pixel clickable border of both labels as well as
// visible air between them. All coordinates are native game pixels.
constexpr int loot_label_padding=3;
constexpr int loot_label_gap=8;
constexpr unsigned world_label_limit=72; // 60 drops plus 12 quiet object names.
inline unsigned loot_label_color(unsigned nativeColor,unsigned effectColor,bool equipment) {
    // Equipment names retain Sigma's rarity colour independently of the
    // effect palette. Runes, gems and supplies keep their category colours.
    constexpr unsigned colors[]={3,4,2,11,1,0};
    return equipment ? (nativeColor<16?nativeColor:0) : (effectColor<std::size(colors)?colors[effectColor]:0);
}
inline unsigned loot_label_priority(unsigned rank,unsigned quality,unsigned tier,bool sacred) {
    // Match the effects' importance first. Break equipment ties by quality,
    // then sacred/tier status; base IDs and trade-price guesses are not value.
    const unsigned q=quality==7?6:quality==5?5:quality==8?4:quality==9?3:quality==6?2:quality==4?1:0;
    return (std::min(rank,4u)*8+q)*8+(sacred?5:std::min(tier,4u));
}
inline bool labels_overlap(const HitRect& a,const HitRect& b,int gap=loot_label_gap) {
    return a.left<b.right+gap && a.right+gap>b.left
        && a.top<b.bottom+gap && a.bottom+gap>b.top;
}

class LabelLayout {
    HitRect area{};
    std::array<HitRect,world_label_limit> occupied{};
    unsigned count=0;

    bool fits(const HitRect& box) const {
        if(box.left<area.left || box.top<area.top || box.right>area.right || box.bottom>area.bottom) return false;
        for(unsigned i=0;i<count;++i) if(labels_overlap(box,occupied[i])) return false;
        return true;
    }

    // At a fixed X, each intersecting label forbids one interval of Y values.
    // Search the gaps in their union, rather than nudging a label pixel by
    // pixel or bouncing it between two neighbours.
    bool vertical(int x,int y,int width,int height,HitRect& result) const {
        struct Interval { int first,last; };
        std::array<Interval,world_label_limit> blocked{};
        unsigned n=0;
        for(unsigned i=0;i<count;++i) {
            const auto& other=occupied[i];
            if(x>=other.right+loot_label_gap || x+width+loot_label_gap<=other.left) continue;
            blocked[n++]={other.top-height-loot_label_gap+1,other.bottom+loot_label_gap-1};
        }
        std::sort(blocked.begin(),blocked.begin()+n,[](auto a,auto b) { return a.first<b.first; });
        const int maximum=area.bottom-height;
        int cursor=area.top,best=0,distance=INT_MAX;
        bool found=false;
        auto offer=[&](int first,int last) {
            last=std::min(last,maximum);
            if(first>last) return;
            const int top=std::clamp(y,first,last),d=std::abs(top-y);
            if(!found || d<distance || (d==distance && top<best)) { found=true;best=top;distance=d; }
        };
        for(unsigned i=0;i<n && cursor<=maximum;++i) {
            offer(cursor,blocked[i].first-1);
            cursor=std::max(cursor,blocked[i].last+1);
        }
        offer(cursor,maximum);
        if(found) result={x,best,x+width,best+height};
        return found;
    }

public:
    explicit LabelLayout(HitRect world):area{world.left+5,world.top+5,world.right-5,world.bottom-5} {}
    HitRect bounds() const { return area; }

    bool place(HitRect wanted,const HitRect* previous,HitRect& result) {
        const int width=wanted.right-wanted.left,height=wanted.bottom-wanted.top;
        if(count==occupied.size() || width<=0 || height<=0
            || width>area.right-area.left || height>area.bottom-area.top) return false;
        const int x=std::clamp(wanted.left,area.left,area.right-width);
        const int y=std::clamp(wanted.top,area.top,area.bottom-height);
        if(previous && previous->right-previous->left==width && previous->bottom-previous->top==height && fits(*previous))
            result=*previous;
        else if(fits({x,y,x+width,y+height})) result={x,y,x+width,y+height};
        else if(!vertical(x,y,width,height,result)) {
            // Only a full-height column needs a neighbouring column. Keep all
            // names inside the visible world and out of inventory/HUD space.
            std::array<int,world_label_limit*2+2> columns{};
            unsigned n=0;
            columns[n++]=area.left;columns[n++]=area.right-width;
            for(unsigned i=0;i<count;++i) {
                columns[n++]=occupied[i].left-loot_label_gap-width;
                columns[n++]=occupied[i].right+loot_label_gap;
            }
            std::sort(columns.begin(),columns.begin()+n,[&](int a,int b) {
                const int da=std::abs(a-x),db=std::abs(b-x);
                return da!=db?da<db:a<b;
            });
            bool found=false;
            for(unsigned i=0;i<n;++i) {
                const int next=columns[i];
                if(next<area.left || next+width>area.right || next==x || (i && next==columns[i-1])) continue;
                if(vertical(next,y,width,height,result)) { found=true;break; }
            }
            // Physical screen capacity is finite. Never draw on top of a
            // different name if no readable space remains; the effect stays.
            if(!found) return false;
        }
        occupied[count++]=result;
        return true;
    }
};

struct LootLabelRequest {
    HitRect wanted{},previous{},placed{};
    unsigned priority=0;
    uint32_t id=0;
    bool hasPrevious=false,visible=false;
    // Object names should shed old edge/column offsets as their sprite moves.
    // Item piles retain their settled layout when a neighbouring drop leaves.
    bool followAnchor=false;
};

inline void arrange_loot_labels(std::span<LootLabelRequest> labels,HitRect world) {
    LabelLayout layout(world);
    const auto area=layout.bounds();
    const unsigned count=unsigned(std::min(labels.size(),size_t(world_label_limit)));
    std::array<unsigned,world_label_limit> parent{},order{};
    std::array<bool,world_label_limit> valid{},done{};
    auto root=[&](unsigned i) { while(parent[i]!=i) i=parent[i];return i; };
    for(unsigned i=0;i<count;++i) {
        auto& label=labels[i];label.visible=false;parent[i]=order[i]=i;
        const int width=label.wanted.right-label.wanted.left,height=label.wanted.bottom-label.wanted.top;
        if(width<=0 || height<=0 || width>area.right-area.left || height>area.bottom-area.top) continue;
        const int x=std::clamp(label.wanted.left,area.left,area.right-width);
        const int y=std::clamp(label.wanted.top,area.top,area.bottom-height);
        label.wanted={x,y,x+width,y+height};valid[i]=true;
        for(unsigned j=0;j<i;++j) if(valid[j] && labels_overlap(label.wanted,labels[j].wanted)) parent[root(i)]=root(j);
    }
    std::sort(order.begin(),order.begin()+count,[&](unsigned a,unsigned b) {
        if(labels[a].priority!=labels[b].priority) return labels[a].priority>labels[b].priority;
        return labels[a].id<labels[b].id;
    });
    auto include=[](HitRect& box,const HitRect& other) {
        box={std::min(box.left,other.left),std::min(box.top,other.top),std::max(box.right,other.right),std::max(box.bottom,other.bottom)};
    };
    auto shift=[](HitRect r,int x,int y) { return HitRect{r.left+x,r.top+y,r.right+x,r.bottom+y}; };
    for(unsigned n=0;n<count;++n) {
        const auto first=order[n];
        if(!valid[first] || done[root(first)]) continue;
        const auto group=root(first);done[group]=true;
        std::array<unsigned,world_label_limit> members{};unsigned total=0;
        for(unsigned i=0;i<count;++i) if(valid[order[i]] && root(order[i])==group) members[total++]=order[i];
        // Stack only naturally colliding names. Independent drops keep their
        // own anchor. Highest importance goes first, at the top of each stack.
        for(unsigned start=0;start<total;) {
            unsigned end=start;int height=0;
            HitRect natural=labels[members[start]].wanted;
            while(end<total) {
                const auto& wanted=labels[members[end]].wanted;
                const int next=height+(end>start?loot_label_gap:0)+wanted.bottom-wanted.top;
                if(next>area.bottom-area.top) break;
                height=next;include(natural,wanted);++end;
            }
            bool retain=true;
            HitRect previous=labels[members[start]].previous;
            for(unsigned i=start;i<end;++i) {
                const auto& label=labels[members[i]];
                if(!label.hasPrevious || label.previous.right-label.previous.left!=label.wanted.right-label.wanted.left
                    || label.previous.bottom-label.previous.top!=label.wanted.bottom-label.wanted.top
                    || (i>start && labels[members[i-1]].previous.bottom+loot_label_gap>label.previous.top)) retain=false;
                if(label.followAnchor && (label.previous.left!=label.wanted.left
                    || (end==start+1 && label.previous.top!=label.wanted.top))) retain=false;
                include(previous,label.previous);
            }
            HitRect placed{};
            if(retain && layout.place(previous,&previous,placed)) {
                for(unsigned i=start;i<end;++i) {
                    auto& label=labels[members[i]];
                    label.placed=shift(label.previous,placed.left-previous.left,placed.top-previous.top);label.visible=true;
                }
            } else {
                const int top=(natural.top+natural.bottom-height)/2;
                if(layout.place({natural.left,top,natural.right,top+height},nullptr,placed)) {
                    int y=placed.top;
                    for(unsigned i=start;i<end;++i) {
                        auto& label=labels[members[i]];
                        label.placed=shift(label.wanted,placed.left-natural.left,y-label.wanted.top);label.visible=true;
                        y=label.placed.bottom+loot_label_gap;
                    }
                }
            }
            start=end;
        }
    }
}
}
