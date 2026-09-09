// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>
#include <chrono>
#include <stdexcept>
#include "native_loot_labels.h"
using namespace mxl::native_loot;
void require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
bool same(HitRect a,HitRect b) { return a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom; }
HitRect shifted(HitRect r,int x,int y) { return {r.left+x,r.top+y,r.right+x,r.bottom+y}; }
LootLabelRequest item(uint32_t id,unsigned rank,unsigned quality,unsigned tier,bool sacred,HitRect wanted) {
    LootLabelRequest result{};result.id=id;result.priority=loot_label_priority(rank,quality,tier,sacred);result.wanted=wanted;return result;
}
void verify(std::span<const LootLabelRequest> items,HitRect world,bool all=true) {
    for(unsigned i=0;i<items.size();++i) {
        const auto& a=items[i];
        if(all) require(a.visible,"an ordinary loot pile lost a label");
        if(!a.visible) continue;
        const auto r=a.placed;
        require(r.left>=world.left+5 && r.right<=world.right-5 && r.top>=world.top+5 && r.bottom<=world.bottom-5,"label escaped world or covered inventory/HUD");
        require(r.right-r.left==a.wanted.right-a.wanted.left && r.bottom-r.top==a.wanted.bottom-a.wanted.top,"layout changed label size");
        for(unsigned j=0;j<i;++j) if(items[j].visible) {
            require(!labels_overlap(r,items[j].placed),"loot names overlap");
            const auto b=items[j].placed;
            const HitRect clickA{r.left-3,r.top-3,r.right+3,r.bottom+3},clickB{b.left-3,b.top-3,b.right+3,b.bottom+3};
            require(!labels_overlap(clickA,clickB,1),"clickable name borders overlap");
        }
    }
}
int main() {
    try {
        const HitRect world=world_input_rect(0,1600,900);
        std::array separate={item(1,1,2,0,false,{200,200,310,228}),item(2,4,7,0,true,{900,500,1180,564})};
        const auto before=separate;
        arrange_loot_labels(separate,world);verify(separate,world);
        for(unsigned i=0;i<separate.size();++i) require(same(separate[i].placed,before[i].wanted),"isolated name moved");

        // Different heights represent multiline names at the accepted four
        // font scales. IDs deliberately disagree with value order.
        std::array pile={item(1,1,2,0,false,{650,400,780,430}),item(2,1,6,4,false,{620,390,810,438}),
                        item(3,2,7,1,false,{590,380,840,448}),item(4,3,7,0,true,{585,370,845,460}),
                        item(5,1,6,1,false,{625,392,805,432})};
        const auto raw=pile;
        arrange_loot_labels(pile,world);verify(pile,world);
        require(pile[3].placed.bottom<pile[2].placed.top,"sacred unique not above tiered unique");
        require(pile[2].placed.bottom<pile[1].placed.top,"unique not above rare equipment");
        require(pile[1].placed.bottom<pile[4].placed.top,"higher rare tier not above lower tier");
        require(pile[4].placed.bottom<pile[0].placed.top,"rare not above ordinary supplies");
        require(loot_label_priority(4,2,0,false)>loot_label_priority(3,7,4,true),"quality displaced a higher effect-importance class");
        require(loot_label_priority(2,6,0,true)>loot_label_priority(2,6,4,false),"sacred equipment tie broken below tiered gear");

        auto reverse=raw;std::reverse(reverse.begin(),reverse.end());
        arrange_loot_labels(reverse,world);
        for(const auto& a:pile) for(const auto& b:reverse) if(a.id==b.id) require(same(a.placed,b.placed),"stack shuffled with native traversal order");
        auto stable=raw;
        for(unsigned i=0;i<stable.size();++i) { stable[i].previous=pile[i].placed;stable[i].hasPrevious=true; }
        arrange_loot_labels(stable,world);verify(stable,world);
        for(unsigned i=0;i<stable.size();++i) require(same(stable[i].placed,pile[i].placed),"settled stack shuffled");
        auto removed=stable;removed[2].wanted={};
        arrange_loot_labels(removed,world);verify(removed,world,false);
        for(unsigned i=0;i<removed.size();++i) if(i!=2) require(same(removed[i].placed,pile[i].placed),"picking up a neighbour shuffled its remaining labels");
        auto camera=stable;
        for(auto& label:camera) { label.wanted=shifted(label.wanted,20,10);label.previous=shifted(label.previous,20,10); }
        arrange_loot_labels(camera,world);verify(camera,world);
        for(unsigned i=0;i<camera.size();++i) require(same(camera[i].placed,shifted(pile[i].placed,20,10)),"stack detached from camera movement");

        // Names at screen edges and in either inventory-open world half.
        for(unsigned panels=0;panels<3;++panels) for(int edge:{0,1}) {
            const auto area=world_input_rect(panels,800,600);
            std::array edgePile={item(1,1,2,0,false,{}),item(2,2,7,0,false,{}),item(3,3,7,0,true,{})};
            for(auto& label:edgePile) label.wanted={edge?area.right-30:area.left-80,edge?530:-25,edge?area.right+160:area.left+110,edge?560:5};
            arrange_loot_labels(edgePile,area);verify(edgePile,area);
            require(edgePile[2].placed.bottom<edgePile[1].placed.top && edgePile[1].placed.bottom<edgePile[0].placed.top,"edge clamping changed value order");
        }

        // A full budget can exceed one column's height. Extra columns must
        // remain readable, clickable and separate instead of clipping a pile.
        std::array<LootLabelRequest,60> crowded{};
        for(unsigned i=0;i<crowded.size();++i) crowded[i]=item(i,1+i%4,2,0,false,{700,410,860,442});
        const auto crowdedRaw=crowded;
        arrange_loot_labels(crowded,world);verify(crowded,world);
        for(const auto& target:crowded) {
            const int x=(target.placed.left+target.placed.right)/2,y=(target.placed.top+target.placed.bottom)/2;
            PickChoice choice;
            choice.offer(99,999,x,y,x,y,false); // Another item's beam may pass behind a name.
            for(unsigned i=0;i<crowded.size();++i) {
                const auto& entry=crowded[i];const int ax=780,ay=460;
                GroundEntry identity{entry.id,10,entry.id+100,1,0,0,{1,1600,900,0,false}};
                const auto r=entry.placed;
                HoverLabel name{identity,{r.left-ax-3,r.top-ay-3,r.right-ax+3,r.bottom-ay+3},100,true};
                if(name.contains(identity,ax,ay,x,y,101)) choice.offer(int(i),entry.id,x,y,ax,ay,true);
            }
            require(choice.index>=0 && choice.index<60 && crowded[choice.index].id==target.id,"moved name selected a different item");
        }
        std::array impossible={item(1,4,7,0,true,{0,0,2000,30})};
        arrange_loot_labels(impossible,world);require(!impossible[0].visible,"oversized label drawn across UI");
        auto covered=crowdedRaw;arrange_loot_labels(covered,world_input_rect(3,800,600));
        for(const auto& label:covered) require(!label.visible,"labels drawn over two open panels");

        // Repeated changing layouts exercise bounded work at the full draw
        // budget. This is an isolated CPU check, not a live game FPS claim.
        const auto start=std::chrono::steady_clock::now();
        for(unsigned frame=0;frame<2000;++frame) {
            auto scene=crowdedRaw;
            for(unsigned i=0;i<scene.size();++i) { scene[i].previous=crowded[i].placed;scene[i].hasPrevious=true; }
            arrange_loot_labels(scene,world);
            if(frame==1999) verify(scene,world);
        }
        const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/2000.;
        std::printf("PASS: non-overlapping value/quality/tier stacks, multiline sizes, stable identities, pickup, camera, panels and 60-item capacity; %.3f us per isolated full-budget layout\n",us);
        return 0;
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
}
