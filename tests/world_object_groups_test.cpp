// SPDX-License-Identifier: GPL-3.0-or-later
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>
#include "world_object_groups.h"
using namespace mxl::native_loot;
namespace {
ObjectIndicators source,other;
ObjectGroups groups,comparison;
void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
ObjectIndicator make(unsigned id,int x,int y,const wchar_t* name=L"Urn",ObjectLook look={ObjectKind::Container,1,5,1,4}) {
    ObjectIndicator e{};e.identity={id,3,100+id,1,0,0,{40,1600,900,0,false}};
    e.x=x;e.y=y;e.look=look;std::wcsncpy(e.name.data(),name,e.name.size()-1);return e;
}
void verify() {
    unsigned total=0;
    for(unsigned g=0;g<groups.markers.count;++g) {
        const auto& marker=groups.markers.entries[g];unsigned members=0;
        int left=INT32_MAX,right=INT32_MIN,top=INT32_MAX,bottom=INT32_MIN;
        bool representative=false;
        for(unsigned i=0;i<source.count;++i) {
            const auto& e=source.entries[i];if(e.group!=g) continue;
            ++members;representative|=same_ground(marker.identity,e.identity);
            left=std::min(left,e.x);right=std::max(right,e.x);top=std::min(top,e.y);bottom=std::max(bottom,e.y);
            check(groups.marker(e)==&marker,"member cannot find its displayed group");
            check(marker.identity.view==e.identity.view && marker.identity.act==e.identity.act,"group crosses a scene boundary");
            check(ObjectGroups::hazardous(marker)==ObjectGroups::hazardous(e),"hazard warning mixed into ordinary loot");
        }
        check(representative && members==marker.members,"lost member, incorrect count or invented click target");
        check(right-left<=object_group_width && bottom-top<=object_group_height,"neighbour chain spread a group too far");
        check(marker.x==left+(right-left)/2 && marker.y==top+(bottom-top)/2,"shared cue off centre");
        total+=members;
    }
    check(total==source.count,"grouping lost an individual openable");
}
void scenarios() {
    groups.rebuild(source);check(groups.markers.count==0,"empty scene gained a marker");
    source.remember(make(1,200,200));groups.rebuild(source);verify();
    check(groups.markers.count==1 && !object_has_label(groups.markers.entries[0]),"single urn display changed");
    source.remember(make(2,220,208));source.remember(make(3,240,212));
    groups.rebuild(source);verify();
    check(groups.markers.count==1 && groups.markers.entries[0].members==3,"close urns not merged");
    check(std::wstring_view(groups.markers.entries[0].name.data())==L"Urns x3","urn count missing");
    check(groups.markers.labels().count==1,"merged ordinary containers lack a count label");
    auto barrel=make(4,230,218,L"Barrel");source.remember(barrel);
    auto chest=make(5,210,209,L"Chest",{ObjectKind::Container,2,1,1,20});source.remember(chest);
    groups.rebuild(source);verify();
    check(std::wstring_view(groups.markers.entries[0].name.data())==L"Containers x5","mixed-container count wrong");
    check(groups.markers.entries[0].identity.id==5 && groups.markers.entries[0].look.priority==20,"important container lost its cue or label click");
    source.remember(make(6,225,211,L"Experience Shrine",{ObjectKind::Shrine,2,3,1,24}));
    groups.rebuild(source);verify();
    check(groups.markers.entries[0].identity.id==6
        && std::wstring_view(groups.markers.entries[0].name.data())==L"Experience Shrine +5","mixed shrine group hid its important member");
    other=source;std::reverse(other.entries.begin(),other.entries.begin()+other.count);
    comparison.rebuild(other);
    check(comparison.markers.count==groups.markers.count,"traversal order changes grouping");
    for(unsigned i=0;i<source.count;++i) for(unsigned j=0;j<other.count;++j)
        if(source.entries[i].identity.id==other.entries[j].identity.id)
            check(source.entries[i].group==other.entries[j].group,"traversal order shuffled members");
    for(unsigned frame=0;frame<120;++frame) {
        const auto old=groups.markers.entries[0];
        for(unsigned i=0;i<source.count;++i) { source.entries[i].x+=2;source.entries[i].y-=1; }
        groups.rebuild(source);verify();
        const auto& now=groups.markers.entries[0];
        check(now.x==old.x+2 && now.y==old.y-1 && now.members==old.members && now.identity.id==old.identity.id,
            "group drifts or reshuffles during camera movement");
    }
    // Objects that have opened are absent from the next native draw capture.
    source.clear();source.remember(make(1,200,200));source.remember(make(2,220,208));
    groups.rebuild(source);verify();check(groups.markers.entries[0].members==2,"opened objects retained in count");
    source.clear();source.remember(make(2,220,208));groups.rebuild(source);verify();
    check(groups.markers.entries[0].members==1 && !object_has_label(groups.markers.entries[0]),"last urn retained a stale group name");
    source.clear();groups.rebuild(source);check(!groups.markers.count,"empty group retained its effect");
    // Cross grid boundaries on either side of zero, including exact limits.
    for(int x:{-65,-64,-1,0,63,64}) for(int y:{-33,-32,-1,0,31,32}) {
        source.clear();source.remember(make(1,x,y));source.remember(make(2,x+64,y+32));
        groups.rebuild(source);verify();check(groups.markers.count==1,"grid boundary split close objects");
    }
    source.clear();
    // A chain and neighbouring rows must remain separate tight groups.
    for(unsigned i=0;i<8;++i) source.remember(make(i,200+int(i)*40,200));
    source.remember(make(20,200,233));groups.rebuild(source);verify();
    check(groups.markers.count==5,"distant containers or separate rows merged");
    source.clear();
    source.remember(make(1,400,300));
    auto danger=make(2,400,300,L"Exploding Barrel",{ObjectKind::Container,1,4,1,3});source.remember(danger);
    auto differentView=make(3,400,300);differentView.identity.view.level=41;source.remember(differentView);
    differentView=make(4,400,300);differentView.identity.act=2;source.remember(differentView);
    groups.rebuild(source);verify();check(groups.markers.count==4,"hazard or scene boundary ignored");
    source.clear();
    std::wstring longName(64,L'W');
    for(unsigned i=0;i<object_indicator_limit;++i) source.remember(make(i,400,300,longName.c_str()));
    groups.rebuild(source);verify();
    const std::wstring_view counted=groups.markers.entries[0].name.data();
    check(groups.markers.count==1 && counted.ends_with(L"x1024") && counted.size()<65,"capacity count overflowed name storage");
    // Benchmark only the added grouping work after capture, with no game calls.
    for(unsigned count:{24u,1024u}) {
        source.clear();
        for(unsigned i=0;i<count;++i) source.remember(make(i,100+int(i%32)*100,100+int(i/32)*48));
        const unsigned frames=count==24?10000:200;
        const auto start=std::chrono::steady_clock::now();
        for(unsigned frame=0;frame<frames;++frame) groups.rebuild(source);
        const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/frames;
        verify();check(groups.markers.count==count,"sparse benchmark unexpectedly merged objects");
        std::printf("Grouping %u separate objects: %.3f us per frame (isolated CPU replay)\n",count,us);
    }
}
}
int main() {
    try { scenarios();std::puts("PASS: close and mixed object groups, count labels, individual identities, camera stability, opening, scene boundaries and full capacity"); }
    catch(const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
}
