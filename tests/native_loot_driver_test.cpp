// SPDX-License-Identifier: GPL-3.0-or-later
// Offline only: exercise the installed 1.13c D2Glide cell/cache/quad path.
// The driver is mapped without its entry point. Only this helper's imports
// and renderer globals are initialized; no game process or file is modified.
#include <windows.h>
#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include "native_loot_cells.h"
#include "d2/structs.h"
#include "glide/texture_manager.h"
#include <memory>
using namespace mxl::native_loot;
using namespace d2gl::d2;
void require(bool ok,const char* why) { if(!ok) {fprintf(stderr,"CHECK FAILED: %s\n",why);throw std::runtime_error(why);} }
struct Info { int small,large,aspect,format;void* data; };
struct Vertex { float x,y;unsigned colour;float q,s,t,unused; };
struct Entry { unsigned index,address,parent,kind,palette,next,prev; };
struct Pool { unsigned bytes,count,used,head,tail,freeHead,freeTail,entries; };
std::vector<unsigned char> ram(16*1024*1024),output(384*768),expected;
unsigned address=0,texWidth=0,texHeight=0,draws=0,downloads=0;
bool additive=false;
std::vector<unsigned char> atlas(256*512*512,0xa7);
std::unique_ptr<d2gl::TextureManager> cache;
d2gl::SubTextureInfo selected{};
unsigned rendererFrame=1;
void __stdcall combine5(unsigned,unsigned,unsigned,unsigned,unsigned) {}
void __stdcall combine7(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned) {}
void __stdcall filter3(unsigned,unsigned,unsigned) {}
void __stdcall constant(unsigned) {}
void __stdcall blend(unsigned src,unsigned dst,unsigned,unsigned) { additive=src==4&&dst==4; }
void dimensions(Info* i) { texWidth=texHeight=1u<<i->large;if(i->aspect<0)texWidth>>=-i->aspect;else texHeight>>=i->aspect; }
void __stdcall download(unsigned tmu,unsigned start,unsigned,Info* i) {
    require(tmu==0,"unexpected TMU");dimensions(i);require(start+texWidth*texHeight<=ram.size(),"texture address overflow");
    memcpy(ram.data()+start,i->data,texWidth*texHeight);++downloads;
    unsigned hash=2166136261u;for(unsigned n=0;n<texWidth*texHeight;++n)hash=(hash^ram[start+n])*16777619u;
    d2gl::g_glide_texture.hash[start]=hash;
}
void __stdcall source(unsigned tmu,unsigned start,unsigned,Info* i) {
    require(tmu==0,"unexpected source TMU");dimensions(i);address=start;
    auto slot=cache->getSubTextureInfo(start,std::max(texWidth,texHeight),texWidth,texHeight,rendererFrame);
    require(slot!=nullptr,"renderer cache exhausted");selected=*slot;
}
void __stdcall quad(unsigned mode,unsigned count,Vertex* v,unsigned stride) {
    require(mode==5&&count==4&&stride==sizeof(Vertex),"unexpected native quad");
    require(additive,"effect did not use additive blend");
    require(v[0].colour==0xffffffff,"effect inherited native tint");
    ++draws;
    const int left=int(v[0].x),right=int(v[2].x),top=int(v[0].y),bottom=int(v[1].y);
    require(right>left&&bottom>top,"native quad is inverted");
    const float scale=float(std::max(texWidth,texHeight))/256.f;
    for(int y=top;y<bottom;++y) for(int x=left;x<right;++x) {
        const float u=v[0].s+(v[2].s-v[0].s)*(x-left+.5f)/(right-left);
        const float t=v[0].t+(v[1].t-v[0].t)*(y-top+.5f)/(bottom-top);
        const int tx=int(u*scale),ty=int(t*scale);
        require(tx>=0&&ty>=0&&tx<int(texWidth)&&ty<int(texHeight),"native texture coordinates out of range");
        if(x>=0&&x<384&&y>=0&&y<768) {
            auto pixel=atlas[(selected.tex_num*512+selected.offset.y+ty)*512+selected.offset.x+tx];
            if(pixel)output[y*384+x]=pixel;
        }
    }
}
void put(unsigned char* image,unsigned rva,void* value) {
    DWORD old;require(VirtualProtect(image+rva,4,PAGE_READWRITE,&old)!=0,"protect helper import");
    *reinterpret_cast<void**>(image+rva)=value;VirtualProtect(image+rva,4,old,&old);
}
template<class T> T& at(unsigned char* image,unsigned rva) {return *reinterpret_cast<T*>(image+rva);}
int main(int argc,char** argv) {
 try {
    setvbuf(stdout,nullptr,_IONBF,0);setvbuf(stderr,nullptr,_IONBF,0);
    require(argc==2,"usage: native_loot_driver_test <installed game directory>");
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    auto dir=std::filesystem::absolute(argv[1]);
    auto cmp=LoadLibraryExW((dir/L"D2CMP.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    auto* driver=reinterpret_cast<unsigned char*>(LoadLibraryExW((dir/L"D2Glide.dll").c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES));
    require(cmp&&driver,"cannot load offline native modules");
    const unsigned char entryBytes[]={0x83,0xec,0x38,0x55,0x56,0x57};
    require(memcmp(driver+0xb190,entryBytes,sizeof(entryBytes))==0,"unsupported native draw build");
    for(auto pair:std::array<std::pair<unsigned,unsigned>,10>{{{0x11000,10069},{0x11004,10067},{0x11008,10015},{0x1100c,10000},{0x11010,10106},{0x11014,10005},{0x11018,10098},{0x1101c,10002},{0x11020,10087},{0x11024,10060}}})
        put(driver,pair.first,reinterpret_cast<void*>(GetProcAddress(cmp,MAKEINTRESOURCEA(pair.second))));
    put(driver,0x111cc,(void*)source);put(driver,0x111d0,(void*)quad);put(driver,0x111d4,(void*)download);
    put(driver,0x111d8,(void*)constant);put(driver,0x111ec,(void*)constant);put(driver,0x1120c,(void*)combine5);put(driver,0x11220,(void*)filter3);
    put(driver,0x11228,(void*)combine5);put(driver,0x11238,(void*)combine7);put(driver,0x1123c,(void*)blend);put(driver,0x1124c,(void*)constant);
    std::array<unsigned,2> settings{};at<void*>(driver,0x15a8c)=settings.data();
    at<unsigned>(driver,0x15a68)=384;at<unsigned>(driver,0x15b04)=768;
    std::vector<unsigned char> scratch(65536),gamma(65536);
    d2gl::g_glide_texture.memory=ram.data();
    cache=std::make_unique<d2gl::TextureManager>(d2gl::SubTextureCounts{{256,64},{128,32},{64,16},{32,8},{16,2},{8,1}},
        [](uint8_t* pixels,const d2gl::SubTextureInfo& slot,uint16_t w,uint16_t h) {
            for(unsigned y=0;y<h;++y)memcpy(atlas.data()+(slot.tex_num*512+slot.offset.y+y)*512+slot.offset.x,pixels+y*w,w);
        });
    for(unsigned i=0;i<gamma.size();++i)gamma[i]=i&255;
    at<void*>(driver,0x17b50)=scratch.data();at<void*>(driver,0x17b4c)=gamma.data();
    std::array<std::vector<Entry>,3> entries;
    unsigned start=0;
    for(unsigned p=0;p<3;++p) {
        // Small pools force native eviction/reuse throughout all animations.
        auto& e=entries[p];e.resize(8);unsigned bytes=65536>>(p*2);
        for(unsigned i=0;i<e.size();++i)e[i]={i,start+i*bytes,0,0,0,i+1<e.size()?unsigned(&e[i+1]):0,i?unsigned(&e[i-1]):0};
        at<Pool>(driver,0x16478+p*32)={bytes,unsigned(e.size()),0,0,0,unsigned(e.data()),unsigned(&e.back()),unsigned(e.data())};
        start+=unsigned(e.size())*bytes;
    }
    using Normalize=void(__stdcall*)(void*,CellFile**,const char*,int,int,int);
    using Release=BOOL(__stdcall*)(CellFile*);
    using Draw=void(__stdcall*)(CellContext*,int,int,int,int,uint8_t*,int,int,void*,void*,void*);
    using HardwareDraw=int(__fastcall*)(CellContext*,int,int,unsigned,int,void*);
    auto normalize=(Normalize)GetProcAddress(cmp,MAKEINTRESOURCEA(10006));
    require(reinterpret_cast<unsigned char*>(normalize)-reinterpret_cast<unsigned char*>(cmp)==0x11ac0,"unsupported native decoder build");
    auto release=(Release)GetProcAddress(cmp,MAKEINTRESOURCEA(10065));
    auto software=(Draw)GetProcAddress(cmp,MAKEINTRESOURCEA(10015));
    auto hardware=(HardwareDraw)(driver+0xb190);
    // D2CMP notifies this driver's cache before the normalized cells are freed.
    ((void(__stdcall*)(void*))GetProcAddress(cmp,MAKEINTRESOURCEA(10067)))(driver+0x9d00);
    struct Fixture {std::vector<uint8_t> bytes;CellFile* file=nullptr;const char* name;};
    std::vector<Fixture> fixtures;
    puts("Generating full catalogue");
    for(unsigned p=1;p<ProfileCount;++p) {
        const auto& profile=profiles[p];
        for(bool glow:{false,true}) {if(glow&&profile.rank<2)continue;fixtures.push_back({make_cells(profile.rank,profile.colour,glow,profile.style),nullptr,profile.name});}
    }
    for(unsigned rank=1;rank<=4;++rank)for(unsigned colour=0;colour<6;++colour)fixtures.push_back({make_landing_cells(rank,colour),nullptr,"landing"});
    for(auto& f:fixtures)normalize(f.bytes.data(),&f.file,__FILE__,__LINE__,-1,0);
    puts("Normalized; drawing through native driver");
    expected.resize(output.size());
    unsigned failures=0,comparisons=0;
    for(unsigned phase=0;phase<48;++phase)for(auto& f:fixtures) {
        rendererFrame=phase+1;
        std::fill(output.begin(),output.end(),0);std::fill(expected.begin(),expected.end(),0);
        const unsigned parts=f.file->numcells/frame_count;
        for(unsigned part=0;part<parts;++part) {
            unsigned index=(phase%24)*parts+part;CellContext c{};c.v113.nCellNo=index;c.v113.pCellFile=f.file;c.v113.pCurGfxCell=f.file->cells[index];
            hardware(&c,192,650,0xffffffff,3,nullptr);
            // Hardware quads use an exclusive bottom edge; the software DC6
            // drawer addresses its inclusive final row.
            software(&c,192,649,0,0,expected.data(),768,384,nullptr,nullptr,nullptr);
        }
        if(output!=expected) {
            ++failures;
            if(failures<=8) {unsigned different=0,first=unsigned(output.size());for(unsigned i=0;i<output.size();++i)if(output[i]!=expected[i]){++different;first=std::min(first,i);}
                printf("MISMATCH %s phase=%u different=%u first=%u,%u actual=%u expected=%u\n",f.name,phase,different,first%384,first/384,output[first],expected[first]);}
        }
        ++comparisons;
    }
    for(auto& f:fixtures)require(release(f.file)!=0,"native cache release failed");
    printf("Native driver: comparisons=%u draws=%u downloads=%u failures=%u\n",comparisons,draws,downloads,failures);
    require(!failures,"native hardware pixels differ from software cells");
    return 0;
 }catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
