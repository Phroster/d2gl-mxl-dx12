// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "native_loot_cells.h"
#include "native_loot_rules.h"
#include "native_loot_layer.h"
#include "d2/structs.h"
using namespace mxl::native_loot;
using namespace d2gl::d2;
void require(bool v,const char* why) { if(!v) throw std::runtime_error(why); }
uint32_t word(const std::vector<uint8_t>& b,size_t p) { require(p+4<=b.size(),"file overrun"); uint32_t v; memcpy(&v,b.data()+p,4); return v; }
int main(int argc,char** argv) {
 try {
    static_assert(std::size(bases)==2468);
    static_assert(offsetof(UnitAny,v110.dwUnitId)==0xc);
    static_assert(offsetof(UnitAny,v110.dwMode)==0x10);
    static_assert(offsetof(UnitAny,v110.pItemData)==0x14);
    static_assert(offsetof(ItemData110,dwFlags)==0x18);
    static_assert(offsetof(CellContext,v113.pCellFile)==0x34);
    static_assert(offsetof(CellContext,v113.pCurGfxCell)==0x3c);
    require(classify(4,3,1242,2,0).rank==2,"perfect gem missing");
    require(classify(4,5,1242,2,0).rank==2,"player drop missing");
    require(classify(4,3,1310,2,0).rank==1,"rune missing");
    require(classify(4,3,0,7,0).colour==1,"unique not gold");
    require(classify(4,3,0,5,0).colour==2,"set not green");
    for(unsigned base=1728;base<=1731;++base) require(classify(4,3,base,2,0).profile==P_ArcaneShard,"arcane shard omitted");
    require(classify(4,3,1732,2,0).profile==P_ArcaneCrystal,"arcane crystal omitted");
    require(classify(4,3,1733,2,0).profile==P_ArcaneCluster,"arcane cluster omitted");
    require(classify(4,3,2465,2,0).profile==P_Ultimate,"untyped endgame reward omitted");
    require(classify(4,3,492,6,0,120).profile==P_ScytheBase,"sacred scythe omitted");
    require(classify(4,3,492,7,0,120).profile==P_ScytheUnique,"unique scythe lost its crescent");
    for(unsigned mode:{0u,1u,2u,4u,6u}) require(!classify(4,mode,1242,7,0).rank,"inventory effect");
    for(unsigned type:{0u,1u,2u,3u,5u}) require(!classify(type,3,1242,7,0).rank,"non-item effect");
    require(!classify(4,3,0xffffffff,7,0).rank,"out-of-range identity");
    unsigned sacred=0;
    for(unsigned base=0;base<std::size(bases);++base) if(bases[base].sacred) {
        require(classify(4,3,base,7,0).rank==3,"sacred unique priority");
        if(!bases[base].profile) require(!classify(4,3,base,4,0).rank,"blue sacred clutter");
        ++sacred;
    }
    require(sacred>100,"incomplete sacred catalog");
    constexpr unsigned cutoff[]={0,31,51,77,90};
    for(unsigned base=0;base<std::size(bases);++base) {
        const auto& b=bases[base];
        require(b.profile<ProfileCount,"catalog profile outside animation set");
        if(b.disabled) continue;
        if(b.gear && !b.jewel && !b.profile) require(!classify(4,3,base,4,0,120).rank,"ordinary blue gear gained effects");
        if(b.tier && !b.profile) {
            require(classify(4,3,base,6,0,cutoff[b.tier]-1).profile==P_Rare,"leveling rare omitted");
            require(!classify(4,3,base,6,0,cutoff[b.tier]).rank,"obsolete tier rare remains lit");
            require(classify(4,3,base,7,0,150).rank>=2,"unique hidden by rare progression");
            require(classify(4,3,base,8,0,150).rank>0,"crafted gear incorrectly treated as obsolete rare");
        }
        if(b.profile==P_GemLesser) {
            require(classify(4,3,base,2,0,49).rank==1,"leveling gem glimmer missing");
            require(!classify(4,3,base,2,0,50).rank,"obsolete imperfect gem still glimmers");
        }
        else if(b.profile) require(classify(4,3,base,2,0,150).rank>0,"catalog special drop lost effect");
    }
    Budget budget;
    for(unsigned i=0;i<12;++i) require(budget.take(1),"minor budget");
    require(!budget.take(1),"uncapped common effects");
    for(unsigned i=0;i<24;++i) require(budget.take(3),"common loot starved major loot");
    require(!budget.take(4),"uncapped major effects");
    GroundQueue queue;
    GroundEntry entry{42,1242,23,1234,0,0,{1,1600,900,0,false}};
    queue.begin(true);
    require(queue.remember(entry,2)&&queue.remember(entry,2),"capture failed");
    require(queue.capturedCount==1&&queue.visibleCount==0,"duplicate or premature effect");
    queue.begin(true);
    require(queue.visibleCount==1&&queue.visible[0].id==42,"missing next-frame identity");
    queue.begin(true);
    require(queue.visibleCount==0,"removed item persisted");
    for(unsigned i=0;i<40;++i) { entry.id=i; queue.remember(entry,1); }
    for(unsigned i=40;i<64;++i) { entry.id=i; require(queue.remember(entry,3),"common items starved beams"); }
    require(queue.capturedCount==36,"queue ignored category budget");
    queue.begin(false);
    require(queue.visibleCount==0&&queue.capturedCount==0,"game transition kept old identities");
    View changed=entry.view; changed.panels=1;
    require(!(changed==entry.view),"panel transition kept old alignment");
    changed=entry.view; changed.level=2;
    require(!(changed==entry.view),"level transition reused old positions");
    // Regression: the live game queued a valid gem but its first unit-table
    // lookup was empty. It must resolve the second local table before giving up.
    struct Candidate { unsigned id,seed,mode; } gem{42,23,3},reused{42,99,3};
    auto matches=[&](const Candidate* p) { return p&&p->id==42&&p->seed==23&&(p->mode==3||p->mode==5); };
    unsigned lookups=0;
    auto lookup=[&](bool second)->Candidate* { ++lookups;return second?&gem:nullptr; };
    require(resolve_ground(lookup,matches)==&gem&&lookups==2,"replicated ground item missing from second table");
    auto collision=[&](bool second) { return second?&gem:&reused; };
    require(resolve_ground(collision,matches)==&gem,"reused first-table ID masked real ground item");
    gem.mode=0;
    require(resolve_ground(lookup,matches)==nullptr,"picked-up item survived two-table lookup");
    gem.mode=3;gem.seed=99;
    require(resolve_ground(lookup,matches)==nullptr,"stale item identity survived lookup");
    PulseHistory pulses;
    require(pulses.age(entry,1000,false)==landing_pulse_ms,"burst fired before landing");
    require(pulses.age(entry,1030,true)==0,"landing did not start burst");
    require(pulses.age(entry,1500,true)==470,"landing burst restarted each draw");
    require(pulses.age(entry,3000,true)>=landing_pulse_ms,"landing burst did not expire");
    auto reopened=entry;reopened.view.panels=1;reopened.localX+=20;
    require(pulses.age(reopened,4000,true)==2970,"camera or panel change replayed landing burst");
    pulses.age(entry,4100,false);
    require(pulses.age(entry,4130,true)==0,"re-dropped item did not pulse on landing");
    pulses.forget(entry);
    require(pulses.age(entry,5000,true)==0,"pickup did not clear pulse identity");
    pulses.clear();
    require(pulses.age(entry,0xfffffff0,true)==0&&pulses.age(entry,0x20,true)==48,"tick wrap broke pulse age");
    pulses.clear();
    for(unsigned i=0;i<513;++i) { auto drop=entry;drop.id=i;require(pulses.age(drop,1000+i,true)==0,"pulse history allocation failed"); }
    auto recent=entry;recent.id=512;
    require(pulses.age(recent,1600,true)==88,"pulse history evicted recent ground item");
    using Normalize=void(__stdcall*)(void*,CellFile**,const char*,int,int,int);
    using Release=BOOL(__stdcall*)(CellFile*);
    // D2CMP #10015 takes framebuffer height followed by byte stride. The old
    // square test buffer could not distinguish them; tall joined art does.
    using NativeDraw=void(__stdcall*)(CellContext*,int,int,int,int,uint8_t*,int height,int stride,void*,void*,void*);
    using Select=BOOL(__stdcall*)(CellContext*,int,int);
    Select select=nullptr;
    Normalize normalize=nullptr; Release release=nullptr; NativeDraw draw=nullptr;
    if(argc>=2) {
        SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
        auto path=std::filesystem::absolute(argv[1])/"D2CMP.dll";
        auto dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        require(dll!=nullptr,"cannot load installed D2CMP in helper");
        normalize=reinterpret_cast<Normalize>(GetProcAddress(dll,MAKEINTRESOURCEA(10006)));
        release=reinterpret_cast<Release>(GetProcAddress(dll,MAKEINTRESOURCEA(10065)));
        draw=reinterpret_cast<NativeDraw>(GetProcAddress(dll,MAKEINTRESOURCEA(10015)));
        select=reinterpret_cast<Select>(GetProcAddress(dll,MAKEINTRESOURCEA(10005)));
        require(normalize&&release&&draw&&select,"native exports absent");
        require(reinterpret_cast<uint8_t*>(normalize)-reinterpret_cast<uint8_t*>(dll)==0x11ac0,"wrong D2CMP build");
    }
    // Regular art gets the requested one-third growth; high-value pillars
    // must be visibly taller while every native texture remains <=256 square.
    require(effect_size(1,Style::Base).width>=48,"base effects did not grow one third");
    require(effect_size(2,Style::Gem).height>=117,"gem effects did not grow one third");
    require(effect_size(2,Style::Beam).height>=176,"unique pillar is not substantially taller");
    require(effect_size(3,Style::Rune).height>=280,"high rune beam too short");
    require(effect_size(4,Style::Rune).height>=336,"great rune beam too short");
    constexpr unsigned bufferWidth=256,bufferHeight=640,drawX=128,drawY=560;
    size_t total=0;unsigned testedFrames=0;
    struct Fixture { std::string name;unsigned rank,colour;Style style;bool bloom,landing; };
    std::vector<Fixture> fixtures;
    for(unsigned profile=1;profile<ProfileCount;++profile) for(bool bloom:{false,true}) {
        const auto& p=profiles[profile];if(bloom&&p.rank<2) continue;
        fixtures.push_back({(bloom?std::string("bloom-"):std::string())+p.name,p.rank,p.colour,p.style,bloom,false});
    }
    for(unsigned rank=1;rank<=4;++rank) for(unsigned colour=0;colour<6;++colour)
        fixtures.push_back({"landing-"+std::to_string(rank)+"-"+std::to_string(colour),rank,colour,Style::Base,false,true});
    for(const auto& pfx:fixtures) {
        const auto rank=pfx.rank,colour=pfx.colour;const bool bloom=pfx.bloom,landing=pfx.landing;
        const unsigned parts=landing?1:cell_parts(rank,pfx.style,bloom),cellCount=frame_count*parts;
        require(parts>=1&&parts<=2,"unbounded native draws");
        auto bytes=landing?make_landing_cells(rank,colour):make_cells(rank,colour,bloom,pfx.style);
        total+=bytes.size();testedFrames+=cellCount;
        require(word(bytes,0)==6&&word(bytes,4)==1&&word(bytes,20)==cellCount,"invalid DC6 header");
        std::array<std::vector<uint8_t>,24> expected;
        const int padding=bloom?bloom_padding:0;
        const auto dimensions=landing?landing_size(rank):effect_size(rank,pfx.style);
        const unsigned fullHeight=dimensions.height+2*padding;
        for(unsigned frame=0;frame<24;++frame) {
            auto& result=expected[frame];result.resize(bufferWidth*bufferHeight);
            unsigned rows=0;
            std::array<bool,256> colours{};
            for(unsigned part=0;part<parts;++part) {
            auto p=word(bytes,24+(frame*parts+part)*4),w=word(bytes,p+4),h=word(bytes,p+8),len=word(bytes,p+28);
            require(p+32+len+3<=bytes.size(),"frame outside allocation");
            require(w>0&&h>0&&w<=256&&h<=256,"sprite exceeds native texture extent");
            const int offsetX=int32_t(word(bytes,p+12)),offsetY=int32_t(word(bytes,p+16));
            const int groundOffset=landing?dimensions.height/2-13:padding+ground_anchor_offset;
            require(offsetX==-int(w)/2&&offsetY==groundOffset-int(rows),"tile seam or world anchor shifted");
            rows+=h;
            require(int(drawX)+offsetX>=0&&int(drawX)+offsetX+int(w)<=bufferWidth
                &&int(drawY)+offsetY-int(h)+1>=0&&int(drawY)+offsetY<bufferHeight,"test sprite outside framebuffer");
            size_t at=p+32; unsigned x=0,row=0,nonzero=0;
            while(at<p+32+len) {
                auto token=bytes[at++];
                if(token==0x80) { require(x==w,"short RLE row"); x=0; ++row; continue; }
                unsigned count=token&0x7f;
                require(count&&x+count<=w&&row<h,"bad RLE run");
                if(!(token&0x80)) {
                    require(at+count<=p+32+len,"literal overrun");
                    for(unsigned i=0;i<count;++i) {
                        require(bytes[at]!=0,"zero literal");
                        colours[bytes[at]]=true;
                        auto& pixel=result[(drawY+offsetY-row)*bufferWidth+drawX+offsetX+x+i];
                        require(pixel==0,"overlapping native tile pixels");
                        pixel=bytes[at++]; ++nonzero;
                    }
                }
                x+=count;
            }
            const bool empty=landing&&(frame==0||frame==frame_count-1);
            require(row==h&&x==0&&(empty?nonzero==0:nonzero>30),"incomplete frame or nonzero landing endpoint");
            }
            require(rows==fullHeight,"tall beam has missing rows");
            if(!bloom&&!landing&&rank>=2) require(std::count(colours.begin(),colours.end(),true)>8,"missing filtered edge shades");
        }
        if(argc>=3) {
            std::filesystem::create_directories(argv[2]);
            std::ofstream f(std::filesystem::path(argv[2])/(pfx.name+".dc6"),std::ios::binary);
            f.write(reinterpret_cast<char*>(bytes.data()),bytes.size());
        }
        if(normalize) {
            CellFile* file=nullptr;
            normalize(bytes.data(),&file,__FILE__,__LINE__,-1,0);
            require(file==reinterpret_cast<CellFile*>(bytes.data()),"normalizer replaced ownership");
            require((file->dwFlags&3)==3&&file->numcells==cellCount,"native normalize failed");
            for(unsigned frame=0;frame<24;++frame) {
                std::vector<uint8_t> output(bufferWidth*bufferHeight+128,0x7b);
                std::fill(output.begin()+64,output.end()-64,0);
                for(unsigned part=0;part<parts;++part) {
                const unsigned index=frame*parts+part;
                auto* cell=file->cells[index];
                require(reinterpret_cast<uint8_t*>(cell)>=bytes.data()&&reinterpret_cast<uint8_t*>(cell)+32<bytes.data()+bytes.size(),"cell pointer outside buffer");
                require(cell->lpParent!=0,"missing native hardware cache");
                CellContext context{}; context.v113.nCellNo=index; context.v113.pCellFile=file; context.v113.pCurGfxCell=cell;
                require(select(&context,0,1)&&context.v113.pCurGfxCell==cell,"native driver frame selection failed");
                draw(&context,drawX,drawY,0,0,output.data()+64,bufferHeight,bufferWidth,nullptr,nullptr,nullptr);
                }
                require(std::all_of(output.begin(),output.begin()+64,[](auto x){return x==0x7b;}),"native draw underrun");
                require(std::all_of(output.end()-64,output.end(),[](auto x){return x==0x7b;}),"native draw overrun");
                require(std::equal(expected[frame].begin(),expected[frame].end(),output.begin()+64),"native decoded pixels differ");
            }
            require(release(file),"native cache cleanup failed");
            require(file->cells[0]->lpParent==0,"cache not freed");
        }
    }
    printf("PASS: 2468 item identities, progression, priorities, budgets, two-table lookup, %u profiles, %u DC6 frames%s; %zu sprite bytes\n",unsigned(ProfileCount)-1,testedFrames,normalize?", installed D2CMP normalization/drawing/cleanup":"",total);
    return 0;
 } catch(const std::exception& e) { fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
