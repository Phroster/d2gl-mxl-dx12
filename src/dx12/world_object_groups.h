// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cwchar>
#include "world_objects.h"

namespace mxl::native_loot {
// Native screen pixels. Bound the whole group, rather than joining chains of
// neighbours into one marker spanning a room. Each real object stays intact.
constexpr int object_group_width=64,object_group_height=32;
struct ObjectGroups {
    ObjectIndicators markers;
    struct Extent {
        int left=0,top=0,right=0,bottom=0;
        int cellX=0,cellY=0;
        bool sameName=true,containers=true;
    };
    std::array<Extent,object_indicator_limit> extents{};
    std::array<int,2048> buckets{};
    std::array<int,object_indicator_limit> next{};

    void clear() { markers.clear(); }
    static bool hazardous(const ObjectIndicator& e) {
        return e.look.kind==ObjectKind::Container && e.look.colour==4 && e.look.priority==3;
    }
    const ObjectIndicator* marker(const ObjectIndicator& member) const {
        if(member.group>=markers.count) return nullptr;
        const auto& result=markers.entries[member.group];
        return result.identity.act==member.identity.act && result.identity.view==member.identity.view?&result:nullptr;
    }
    static const wchar_t* plural(std::wstring_view name) {
        if(name==L"Urn") return L"Urns";
        if(name==L"Barrel") return L"Barrels";
        if(name==L"Chest") return L"Chests";
        if(name==L"Hidden Stash") return L"Hidden Stashes";
        return nullptr;
    }
    static int cell(int coordinate,int size) {
        return coordinate/size-(coordinate<0 && coordinate%size!=0);
    }
    static unsigned bucket(int x,int y) {
        return ((uint32_t(x)*0x9e3779b1u)^(uint32_t(y)*0x85ebca6bu))&2047u;
    }
    void rebuild(ObjectIndicators& source) {
        clear();
        if(!source.count) return;
        buckets.fill(-1);
        std::array<unsigned,object_indicator_limit> order{};
        for(unsigned i=0;i<source.count;++i) order[i]=i;
        // Stable identities make membership independent of native traversal
        // order and camera translation. No history, unit queries or allocation.
        std::sort(order.begin(),order.begin()+source.count,[&](unsigned a,unsigned b) {
            return source.entries[a].identity.id<source.entries[b].identity.id;
        });
        for(unsigned n=0;n<source.count;++n) {
            auto& entry=source.entries[order[n]];
            unsigned best=markers.count;int64_t distance=INT64_MAX;
            const int cx=cell(entry.x,object_group_width),cy=cell(entry.y,object_group_height);
            // A group's first member stays in its original grid cell. The
            // bounded extent means only these nine nearby cells can qualify.
            for(int y=cy-1;y<=cy+1;++y) for(int x=cx-1;x<=cx+1;++x) {
                for(int index=buckets[bucket(x,y)];index>=0;index=next[index]) {
                    const unsigned g=unsigned(index);
                    const auto& group=markers.entries[g];const auto& box=extents[g];
                    if(box.cellX!=x || box.cellY!=y || group.identity.act!=entry.identity.act
                        || !(group.identity.view==entry.identity.view) || hazardous(group)!=hazardous(entry)) continue;
                    if(std::max(box.right,entry.x)-std::min(box.left,entry.x)>object_group_width
                        || std::max(box.bottom,entry.y)-std::min(box.top,entry.y)>object_group_height) continue;
                    const int64_t dx=int64_t(entry.x)*2-box.left-box.right,dy=int64_t(entry.y)*2-box.top-box.bottom;
                    const int64_t score=dx*dx+4*dy*dy;
                    if(score<distance || (score==distance && g<best)) { distance=score;best=g; }
                }
            }
            entry.group=best;
            if(best==markers.count) {
                markers.entries[best]=entry;markers.entries[best].members=1;
                extents[best]={entry.x,entry.y,entry.x,entry.y,cx,cy,true,entry.look.kind==ObjectKind::Container};
                auto& head=buckets[bucket(cx,cy)];next[best]=head;head=int(best);
                ++markers.count;
            } else {
                auto& group=markers.entries[best];auto& box=extents[best];
                box.sameName=box.sameName && std::wstring_view(group.name.data())==entry.name.data();
                box.containers=box.containers && entry.look.kind==ObjectKind::Container;
                box.left=std::min(box.left,entry.x);box.right=std::max(box.right,entry.x);
                box.top=std::min(box.top,entry.y);box.bottom=std::max(box.bottom,entry.y);
                const unsigned members=group.members+1;
                // The count label opens the most important remaining member.
                // Native sprites and the shared glow still allow separate picks.
                if(entry.look.priority>group.look.priority) group=entry;
                group.members=members;
            }
        }
        for(unsigned g=0;g<markers.count;++g) {
            auto& group=markers.entries[g];const auto& box=extents[g];
            group.group=g;
            group.x=box.left+(box.right-box.left)/2;group.y=box.top+(box.bottom-box.top)/2;
            if(group.members<2) continue;
            if(!group.look.pulseRank) group.look.pulseRank=1;
            const auto original=group.name;
            const wchar_t* name=original.data();
            const wchar_t* format=L"%.*ls x%u";
            unsigned number=group.members;
            if(box.sameName) { if(const auto* namePlural=plural(name)) name=namePlural; }
            else if(box.containers) name=L"Containers";
            else { format=L"%.*ls +%u";--number; }
            // Reserve room for the count even with a full-length native name.
            std::swprintf(group.name.data(),group.name.size(),format,int(group.name.size()-10),name,number);
            group.name.back()=0;
        }
    }
};
// Preserve which real objects are covered by the last successfully drawn
// names. Native hover text can be drawn before the next frame's world pass
// finishes; its suppression must not depend on partial new-frame grouping.
struct ObjectNameCoverage {
    struct Member { GroundEntry identity{};unsigned label=0; };
    std::array<Member,object_indicator_limit> members{};
    std::array<GroundEntry,object_label_limit> leaders{};
    unsigned count=0;
    void clear() { count=0; }
    void add(const ObjectIndicators& source,const ObjectIndicator& group,unsigned label) {
        if(label>=leaders.size()) return;
        leaders[label]=group.identity;
        for(unsigned i=0;i<source.count && count<members.size();++i) {
            const auto& entry=source.entries[i];
            if(entry.group==group.group) members[count++]={entry.identity,label};
        }
    }
};
}
