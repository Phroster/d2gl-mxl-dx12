// SPDX-License-Identifier: GPL-3.0-or-later
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include "world_objects.h"
#include "native_loot_labels.h"
using namespace mxl::native_loot;
void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
int main() {
    try {
        ObjectFacts chest{0,8,4,1,2,5,true,true,false,false};
        check(object_look(chest).rank==1,"normal chest too loud");
        check(!object_has_label(object_look(chest)),"ordinary chest gained a cluttering label");
        auto locked=chest;locked.locked=true;
        check(object_look(locked).rank>object_look(chest).rank,"locked chest not distinguished");
        check(object_has_label(object_look(locked)),"important chest missing its name");
        auto special=chest;special.classId=397;
        check(object_look(special).pulseRank==2,"special chest missing stronger glow");
        for(unsigned mode=1;mode<8;++mode) { auto f=chest;f.mode=mode;check(!object_look(f).rank,"used/open container marked"); }
        auto invalid=chest;invalid.mode=8;check(!object_look(invalid).rank,"out-of-range animation mode");
        invalid=chest;invalid.selectable=false;check(!object_look(invalid).rank,"non-interactive object marked");
        invalid=chest;invalid.drawn=false;check(!object_look(invalid).rank,"invisible object marked");
        invalid=chest;invalid.door=true;check(!object_look(invalid).rank,"door clutter");
        for(unsigned op:{0u,8u,11u,13u,15u}) {
            auto f=chest;f.subclass=0;f.operate=op;
            check(!object_look(f).rank,"scenery or portal highlighted as treasure");
        }
        for(unsigned op:{3u,5u,7u,30u,68u}) {
            auto f=chest;f.subclass=0;f.operate=op;
            const auto look=object_look(f);
            check(look.pulseRank==1 && !object_has_label(look),"urn or barrel needs only a quiet effect");
            if(op==7 || op==30 || op==68) check(look.colour==4,"hazard mistaken for a gold treasure marker");
            f.mode=2;check(!object_look(f).rank,"broken urn or barrel still highlighted");
        }
        auto shrine=chest;shrine.subclass=1;shrine.operate=2;
        check(object_look(shrine).kind==ObjectKind::Shrine && object_look(shrine).rank==2,"shrine not visible");
        shrine.mode=2;check(!object_look(shrine).rank,"spent shrine still marked");
        auto waypoint=chest;waypoint.subclass=64;waypoint.operate=23;waypoint.mode=2;
        check(object_look(waypoint).rank==1 && !object_look(waypoint).pulseRank,"waypoint missing or too loud");
        ObjectIndicators queue;
        auto make=[&](unsigned id,ObjectFacts f) {
            ObjectIndicator e{};e.identity={id,f.classId,123,456,0,0,{40,1600,900,0,false}};
            e.x=700+int(id);e.y=400;e.name[0]=L'C';e.look=object_look(f);return e;
        };
        for(unsigned i=0;i<100;++i) queue.remember(make(i,chest));
        check(queue.count==12,"object draw budget exceeded");
        queue.remember(make(999,special));
        check(std::any_of(queue.entries.begin(),queue.entries.end(),[](auto& e){return e.identity.id==999;}),"valuable object crowded out");
        auto duplicate=make(999,special);duplicate.x=300;queue.remember(duplicate);
        check(queue.count==12,"multi-part object duplicated");
        ObjectIndicators reversed;
        reversed.remember(make(999,special));
        for(int i=99;i>=0;--i) reversed.remember(make(unsigned(i),chest));
        // Reconstruct without moving one entry, then compare retained identities.
        ObjectIndicators forward;for(unsigned i=0;i<100;++i) forward.remember(make(i,chest));forward.remember(make(999,special));
        std::array<unsigned,12> a{},b{};
        for(unsigned i=0;i<12;++i){a[i]=forward.entries[i].identity.id;b[i]=reversed.entries[i].identity.id;}
        std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());check(a==b,"draw order changes retained markers");
        queue.clear();check(queue.count==0,"markers retained into an empty frame");
        unsigned last=object_pulse_frame(0,7);
        for(unsigned t=110;t<100000;t+=110) {
            const unsigned frame=object_pulse_frame(t,7);
            check(frame>=9 && frame<=20 && std::abs(int(frame)-int(last))<=1,"pulse loop flashes or overruns asset");last=frame;
        }
        std::array<LootLabelRequest,world_label_limit> labels{};
        for(unsigned i=0;i<labels.size();++i) {
            auto& l=labels[i];l.id=i;l.priority=i<60?loot_label_priority(1,2,0,false):24;
            const int x=40+int(i%12)*125,y=80+int(i/12)*100;
            l.wanted={x,y,x+110,y+24};
        }
        arrange_loot_labels(labels,world_input_rect(0,1600,900));
        for(unsigned i=0;i<labels.size();++i) {
            check(labels[i].visible,"object capacity displaced a loot label");
            for(unsigned j=0;j<i;++j) check(!labels_overlap(labels[i].placed,labels[j].placed),"object/loot labels overlap");
        }
        std::array<LootLabelRequest,2> stack{};
        stack[0].wanted=stack[1].wanted={300,300,420,330};stack[0].priority=24;stack[1].priority=loot_label_priority(1,2,0,false);
        arrange_loot_labels(stack,world_input_rect(0,800,600));
        check(stack[1].placed.bottom<stack[0].placed.top,"object name takes priority over actual loot");
        arrange_loot_labels(labels,world_input_rect(3,800,600));
        for(const auto& l:labels) check(!l.visible,"names cover inventory panels");
        const auto start=std::chrono::steady_clock::now();
        uint64_t visits=0;
        for(unsigned frame=0;frame<100000;++frame) {
            queue.clear();
            for(unsigned i=0;i<24;++i) { queue.remember(make(i,chest));visits+=queue.count; }
        }
        const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/100000;
        std::printf("PASS: object states, importance, empty frames, bounded retention, pulse continuity and 72 mixed labels; %.3f us per 24-object capture (%llu)\n",us,visits);
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
}
