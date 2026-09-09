// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include "native_loot_palette.h"
#include "native_loot_rules.h"

namespace mxl::native_loot {
constexpr unsigned frame_count = 24;
constexpr int bloom_padding = 12;
// Five extra pixels below the old floor ring accommodate the larger aura.
// Compensate in the native offset so the ring stays at the item's old anchor.
constexpr int ground_anchor_offset = 5;
constexpr int native_tile_extent = 256;
struct EffectSize { int width,height; };
inline EffectSize effect_size(unsigned rank,Style style) {
    // Regular effects grow by one third. Beams have extra headroom beyond that
    // increase; their category symbols keep the same proportional enlargement.
    constexpr int widths[]={0,48,96,128,176},heights[]={0,38,118,214,352};
    EffectSize size{widths[rank],heights[rank]};
    if(rank==2 && style==Style::Beam) size={112,192};
    if(rank==2 && style==Style::Rune) size={96,152};
    if(rank==3 && (style==Style::Beam || style==Style::Rune || style==Style::Scythe || style==Style::Relic)) size={144,288};
    return size;
}
inline unsigned cell_parts(unsigned rank,Style style,bool bloom) {
    return unsigned(effect_size(rank,style).height+(bloom?2*bloom_padding:0)+native_tile_extent-1)/native_tile_extent;
}
inline uint8_t faded_light(uint8_t colour,float strength) {
    int best=0x7fffffff;uint8_t match=0;
    for(unsigned i=0;i<unit_palette.size();++i) {
        int error=0;
        for(unsigned channel=0;channel<3;++channel) {
            const int delta=int(unit_palette[i][channel])-int(unit_palette[colour][channel]*strength);
            error+=delta*delta;
        }
        if(error<best) { best=error;match=uint8_t(i); }
    }
    return match;
}
inline void append_cell(std::vector<uint8_t>& file,unsigned index,const std::vector<uint8_t>& pixels,
    int w,int fullHeight,int bottom,int tileHeight,int offsetY)
{
    std::vector<uint8_t> rle;
    for(int y=fullHeight-1-bottom;y>=fullHeight-bottom-tileHeight;--y) {
        for(int x=0;x<w;) {
            const bool transparent=pixels[y*w+x]==0;
            const int start=x++;
            while(x<w&&x-start<127&&(pixels[y*w+x]==0)==transparent) ++x;
            rle.push_back(uint8_t(x-start)|(transparent?0x80:0));
            if(!transparent) rle.insert(rle.end(),pixels.begin()+y*w+start,pixels.begin()+y*w+x);
        }
        rle.push_back(0x80);
    }
    const size_t pos=file.size();file.resize(pos+32+rle.size()+3);
    auto word=[&](size_t p,uint32_t v) { std::memcpy(file.data()+p,&v,4); };
    word(24+index*4,uint32_t(pos));word(pos,0);word(pos+4,w);word(pos+8,tileHeight);
    word(pos+12,uint32_t(-w/2));word(pos+16,uint32_t(offsetY));
    word(pos+20,0);word(pos+24,0);word(pos+28,uint32_t(rle.size()));
    std::copy(rle.begin(),rle.end(),file.begin()+pos+32);
    std::fill(file.end()-3,file.end(),0xee);
}
// Author at 4x the native pixel density on a canvas at least 1024 square, with
// additional vertical space for tall beams. Reduce with
// area filtering into premultiplied native palette colours for additive light.
// The final image is sliced into native cells, each at most 256 pixels tall.
struct EffectCanvas {
    static constexpr int extent=1024, scale=4;
    int width,height,canvasHeight,originX,originY;
    std::vector<uint8_t> pixels;
    EffectCanvas(int w,int h):width(w),height(h),canvasHeight(std::max(extent,h*scale)),originX((extent-w*scale)/2),
        originY((canvasHeight-h*scale)/2),pixels(extent*canvasHeight) {}
    void dot(float x,float y,uint8_t colour) {
        const float cx=originX+(x+.5f)*scale,cy=originY+(y+.5f)*scale;
        const float radius=.58f*scale;
        const int left=std::max(0,int(std::floor(cx-radius))),right=std::min(extent-1,int(std::ceil(cx+radius)));
        const int top=std::max(0,int(std::floor(cy-radius))),bottom=std::min(canvasHeight-1,int(std::ceil(cy+radius)));
        for(int py=top;py<=bottom;++py) for(int px=left;px<=right;++px) {
            const float dx=px+.5f-cx,dy=py+.5f-cy;
            if(dx*dx+dy*dy<=radius*radius) pixels[py*extent+px]=colour;
        }
    }
    std::vector<uint8_t> reduce() const {
        static const auto lookup=[] {
            std::array<uint8_t,32768> table{};
            for(unsigned i=1;i<table.size();++i) {
                const int r=((i>>10)&31)*8+4,g=((i>>5)&31)*8+4,b=(i&31)*8+4;
                int best=0x7fffffff;
                for(unsigned p=0;p<unit_palette.size();++p) {
                    const auto& rgb=unit_palette[p];
                    const int dr=int(rgb[0])-r,dg=int(rgb[1])-g,db=int(rgb[2])-b;
                    const int error=dr*dr+dg*dg+db*db;
                    if(error<best) { best=error;table[i]=uint8_t(p); }
                }
            }
            return table;
        }();
        std::vector<uint8_t> output(width*height);
        for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
            unsigned r=0,g=0,b=0;
            for(int sy=0;sy<scale;++sy) for(int sx=0;sx<scale;++sx) {
                const auto& rgb=unit_palette[pixels[(originY+y*scale+sy)*extent+originX+x*scale+sx]];
                r+=rgb[0];g+=rgb[1];b+=rgb[2];
            }
            constexpr unsigned samples=scale*scale;
            output[y*width+x]=lookup[((r/samples)>>3)*1024+((g/samples)>>3)*32+((b/samples)>>3)];
        }
        return output;
    }
};
// Bake a blurred bright-pass into an additive DC6 sprite once at startup.
// Both layers use the native world drawer, so foreground geometry covers them.
inline std::vector<uint8_t> bloom_pixels(const std::vector<uint8_t>& pixels,
    int w, int h, unsigned rank, unsigned colour)
{
    const int bw=w+2*bloom_padding, bh=h+2*bloom_padding;
    std::vector<float> source(bw*bh), horizontal(bw*bh), blurred(bw*bh);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const auto& rgb=unit_palette[pixels[y*w+x]];
        const float luminance=(rgb[0]+rgb[1]+rgb[2])/765.f;
        source[(y+bloom_padding)*bw+x+bloom_padding]=luminance*luminance;
    }
    const float sigma=rank==2?3.5f:rank==3?5.f:6.f;
    const int radius=int(std::ceil(3*sigma));
    std::vector<float> weights(2*radius+1);
    float sum=0;
    for(int d=-radius;d<=radius;++d) sum+=(weights[d+radius]=std::exp(-float(d*d)/(2*sigma*sigma)));
    for(auto& weight:weights) weight/=sum;
    for(int y=0;y<bh;++y) for(int x=0;x<bw;++x)
        for(int d=std::max(-radius,-x);d<=std::min(radius,bw-1-x);++d)
            horizontal[y*bw+x]+=source[y*bw+x+d]*weights[d+radius];
    for(int y=0;y<bh;++y) for(int x=0;x<bw;++x)
        for(int d=std::max(-radius,-y);d<=std::min(radius,bh-1-y);++d)
            blurred[y*bw+x]+=horizontal[(y+d)*bw+x]*weights[d+radius];
    // Nearest colours in the verified native unit palette, at 5/255 steps.
    // Dark shades keep the halo translucent with the game's ONE+ONE blend.
    static const auto ramps=[] {
        std::array<std::array<uint8_t,33>,6> result{};
        constexpr float hues[6][3]={{.42f,.64f,1},{1,.73f,.3f},{.32f,1,.2f},{.65f,.25f,1},{1,.22f,.08f},{1,1,1}};
        for(unsigned c=0;c<6;++c) for(unsigned level=1;level<=32;++level) {
            float best=1e9f;
            for(unsigned index=0;index<256;++index) {
                float error=0;
                for(unsigned channel=0;channel<3;++channel) {
                    const float delta=unit_palette[index][channel]-level*5*hues[c][channel];error+=delta*delta;
                }
                if(error<best) { best=error;result[c][level]=uint8_t(index); }
            }
        }
        return result;
    }();
    constexpr unsigned dither[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    std::vector<uint8_t> output(bw*bh);
    for(int y=0;y<bh;++y) for(int x=0;x<bw;++x) {
        // Fade the extreme border rather than expose a rectangular sprite edge.
        const float edge=std::min(1.f,float(std::min({x,y,bw-1-x,bh-1-y}))/3.f);
        const float value=blurred[y*bw+x]*(rank==2?230.f:280.f)*edge;
        const unsigned level=std::min(32u,unsigned(value/5.f+(dither[y&3][x&3]+.5f)/16.f));
        output[y*bw+x]=ramps[std::min(colour,5u)][level];
    }
    return output;
}
// Actual DC6 animations, consumed by D2CMP/D2Gfx. No overlay/UI drawing API.
inline std::vector<uint8_t> make_cells(unsigned rank, unsigned colour = 0, bool bloom = false, Style style=Style::Beam)
{
    rank = std::clamp(rank, 1u, 4u);
    const auto size=effect_size(rank,style);
    const int w=size.width,h=size.height;
    const unsigned parts=cell_parts(rank,style,bloom),cells=frame_count*parts;
    constexpr uint8_t lights[]={158,111,133,155,98,255},dims[]={154,88,129,143,80,25};
    const uint8_t light=lights[std::min(colour,5u)],dim=dims[std::min(colour,5u)];
    std::vector<uint8_t> file(24 + cells * 4);
    auto word = [&](size_t pos, uint32_t v) { std::memcpy(file.data() + pos, &v, 4); };
    word(0, 6); word(4, 1); word(8, 0); word(12, 0xeeeeeeee);
    word(16, 1); word(20, cells);
    for (unsigned frame = 0; frame < frame_count; ++frame) {
        std::vector<uint8_t> pixels(w * h);
        EffectCanvas artwork(w,h);
        auto dot = [&](float x, float y, uint8_t c) {
            if(rank>=2) artwork.dot(x,y,c);
            else if (x >= 0 && x < w && y >= 0 && y < h) pixels[int(y)*w+int(x)] = c;
        };
        const float t = float(frame) / frame_count;
        const float tau = 6.28318530718f;
        const int cx = w/2, floor = h-18;
        const float pulse = .5f - .5f*std::cos(t*tau*2);
        const float groundStrength=.3f+.7f*pulse;
        const uint8_t ringLight=faded_light(light,groundStrength);
        const uint8_t ringWhite=faded_light(255,groundStrength);
        auto star = [&](float x, float y, int radius) {
            for (int i=-radius;i<=radius;++i) {
                const uint8_t c = std::abs(i)<=1 ? 255 : light;
                dot(x+i,y,c); dot(x,y+i,c);
            }
            if (radius >= 3) {
                const int diagonal=std::max(1,radius/2);
                for (int i=-diagonal;i<=diagonal;++i) { dot(x+i,y+i,light); dot(x+i,y-i,light); }
                dot(x,y,255);
            }
        };
        auto line=[&](float x1,float y1,float x2,float y2,uint8_t colour) {
            const int steps=std::max(1,int(std::ceil(std::max(std::abs(x2-x1),std::abs(y2-y1))*4)));
            for(int i=0;i<=steps;++i) { const float u=float(i)/steps;dot(x1+(x2-x1)*u,y1+(y2-y1)*u,colour); }
        };
        auto ellipse=[&](float x,float y,float rx,float ry,float phase,uint8_t colour) {
            for(unsigned i=0;i<240;++i) { const float a=i*tau/240+phase;dot(x+std::cos(a)*rx,y+std::sin(a)*ry,colour); }
        };
        auto diamond=[&](float x,float y,float rx,float ry) {
            line(x,y-ry,x+rx,y,light);line(x+rx,y,x,y+ry,light);
            line(x,y+ry,x-rx,y,light);line(x-rx,y,x,y-ry,light);
            line(x,y-ry,x,y+ry,dim);line(x-rx,y,x+rx,y,dim);dot(x,y-ry,255);
        };
        const float radius = (rank==1 ? 11.f : rank==2 ? 21.f : rank==3 ? 29.f : 39.f)*4.f/3.f;
        // A brighter floor halo and two orbiting rings give the beam a base.
        // Ordinary runes/bases retain a sparse single ring.
        if (rank>=2) {
            for (int y=-15;y<=15;++y) for (int x=-int(radius);x<=int(radius);++x) {
                const float r = x*x/(radius*radius) + y*y/(radius*radius*.085f);
                if (r<1.f && ((x+y+int(frame))&3)==0) dot(cx+x,floor+y,r<.32f?light:dim);
            }
        }
        const int ringSamples=rank>=2?640:160;
        for (int a = 0; a < ringSamples; ++a) {
            const float angle = a*tau/ringSamples;
            // Smoothly expand and contract every 600ms, without a ring-size
            // jump at the loop boundary. The 792ms landing burst is wider.
            const float r = radius*(.78f+.22f*pulse);
            const float x=cx+std::cos(angle)*r, y=floor+std::sin(angle)*r*.28f;
            dot(x,y,(a/(ringSamples/160)+frame*3)%16<4?ringWhite:ringLight);
            if (rank>=2) {
                dot(x,y-1,ringLight);
                dot(cx+std::cos(angle)*r*.75f,floor+std::sin(angle)*r*.21f,dim);
            }
        }
        // Uniques and high runes get unmistakable pillars. Top-value drops
        // get the tallest cores, double helixes and an orbiting star crown.
        if (rank>=2 && (style==Style::Beam || style==Style::Relic || rank==4
            || style==Style::Rune || (style==Style::Scythe && rank>=3))) {
            const int top=rank==2?18:22;
            for (int y=top;y<floor-2;++y) {
                const float taper=float(y-top)/float(floor-top);
                const int core=rank==2?2:rank==3?4:5;
                const int spread=core+4+int(taper*(rank*2.5f)+pulse*2.f);
                for (int x = -spread; x <= spread; ++x) {
                    if (std::abs(x)<=core) dot(cx+x,y,255);
                    else if (std::abs(x)<=core+3) dot(cx+x,y,light);
                    else if ((x+y+int(frame))%3!=0) dot(cx+x,y,dim);
                }
                if (rank>=3) {
                    const float wave=std::sin(t*tau-taper*tau*2)*(13.f+rank*3.f);
                    dot(cx+wave,y,light); dot(cx-wave,y,light);
                    if (rank==4) { dot(cx+wave+1,y,light); dot(cx-wave-1,y,light); }
                }
            }
            star(cx,top,rank==2?7:rank==3?11:14);
            if (rank>=3) {
                for(int s=0;s<5;++s) {
                    const float a=t*tau+s*tau/5;
                    const float x=cx+std::cos(a)*radius*.7f,y=top+24+std::sin(a)*10;
                    star(x,y,3+int(2*(.5f+.5f*std::sin(a+t*tau))));
                }
            }
            if (rank==4) star(cx,floor-34,10+int(pulse*4));
        }
        // Distinct silhouettes make the category readable without item labels.
        // All geometry is baked into these same bounded native sprite draws.
        const float lift=(rank==1?6.f:rank==2?28.f:rank==3?58.f:88.f)*4.f/3.f;
        const float cy=floor-lift+std::sin(t*tau)*1.5f;
        const float size=(rank==1?4.f:rank==2?14.f:rank==3?22.f:29.f)*4.f/3.f;
        switch(style) {
        case Style::Rune:
            ellipse(cx,cy,size,size*.7f,0,light);
            for(int i=0;i<3;++i) {
                const float a=t*tau+i*tau/3,b=a+tau/3;
                line(cx+std::cos(a)*size,cy+std::sin(a)*size*.7f,cx+std::cos(b)*size,cy+std::sin(b)*size*.7f,light);
            }
            line(cx,cy-size*.65f,cx,cy+size*.65f,255);
            line(cx,cy,cx+size*.5f,cy-size*.3f,255);break;
        case Style::Gem:
            diamond(cx,cy,size*.75f,size*1.15f);
            if(rank>=2) for(int i=0;i<3;++i) {
                const float a=t*tau+i*tau/3;
                diamond(cx+std::cos(a)*size*1.6f,cy+std::sin(a)*size*.45f,3,5);
            }
            break;
        case Style::Arcane:
            for(int i=0;i<5;++i) {
                const float a=t*tau+i*tau/5;
                const float r=size*(.65f+.3f*std::sin(a*2));
                diamond(cx+std::cos(a)*r,cy+std::sin(a)*r*.9f,2+rank,4+rank);
            }
            star(cx,cy,rank+2);break;
        case Style::Shrine:
            for(int i=0;i<6;++i) {
                const float a=t*tau*.5f+i*tau/6,b=a+tau/3;
                line(cx+std::cos(a)*size,cy+std::sin(a)*size,cx+std::cos(b)*size,cy+std::sin(b)*size,light);
                star(cx+std::cos(a)*size,cy+std::sin(a)*size,1);
            }
            ellipse(cx,cy,size*1.2f,size*1.2f,0,dim);break;
        case Style::Signet:
            ellipse(cx,cy,size*(.3f+.7f*std::abs(std::cos(t*tau))),size,0,light);
            line(cx,cy-size*.65f,cx,cy+size*.65f,255);
            line(cx-size*.3f,cy-size*.4f,cx+size*.3f,cy-size*.4f,light);break;
        case Style::Rare:
            for(int i=0;i<2;++i) {
                const float y=cy+i*(rank==1?4.f:9.f);
                line(cx-size,y+size*.5f,cx,y,light);line(cx,y,cx+size,y+size*.5f,light);
            }
            break;
        case Style::Base:
            for(int s:{-1,1}) {
                line(cx+s*size,floor-7,cx+s*size,floor-1,light);
                line(cx+s*size,floor-7,cx+s*(size-4),floor-7,255);
                line(cx+s*size,floor-1,cx+s*(size-4),floor-1,light);
            }
            star(cx,floor-size,3);break;
        case Style::Scythe:
            line(cx-size*.3f,cy+size*1.3f,cx+size*.2f,cy-size*.8f,light);
            for(int i=0;i<240;++i) {
                const float a=-2.4f+i*3.6f/240;
                dot(cx+std::cos(a)*size,cy-size*.25f+std::sin(a)*size*.75f,light);
                dot(cx+std::cos(a)*size*.78f,cy-size*.25f+std::sin(a)*size*.55f,255);
            }
            break;
        case Style::Relic:
            for(int i=0;i<8;++i) {
                const float a=t*tau*.3f+i*tau/8;
                line(cx+std::cos(a)*size*.4f,cy+std::sin(a)*size*.4f,cx+std::cos(a)*size,cy+std::sin(a)*size,light);
                star(cx+std::cos(a)*size,cy+std::sin(a)*size,3);
            }
            ellipse(cx,cy,size*.8f,size*.8f,0,light);
            for(int side:{-1,1}) line(cx+side*size,floor-12,cx+side*size,cy,dim);
            break;
        case Style::Quest:
            diamond(cx,cy,size,size);
            line(cx-size*1.3f,cy,cx+size*1.3f,cy,light);
            line(cx,cy-size*1.3f,cx,cy+size*1.3f,light);star(cx,cy,rank+1);break;
        case Style::Orb:
            ellipse(cx,cy,size,size,0,light);
            ellipse(cx,cy,size*1.3f,size*.3f,t*tau,dim);
            ellipse(cx,cy,size*.3f,size*1.3f,t*tau,light);star(cx,cy,rank+1);break;
        case Style::Treasure:
            line(cx-size,floor-9,cx-size,floor-size,light);line(cx+size,floor-9,cx+size,floor-size,light);
            line(cx-size,floor-9,cx+size,floor-9,light);line(cx-size,floor-size,cx+size,floor-size,255);
            line(cx-size,floor-size,cx-size*.7f,floor-size*1.4f,light);
            line(cx+size,floor-size,cx+size*.7f,floor-size*1.4f,light);
            line(cx-size*.7f,floor-size*1.4f,cx+size*.7f,floor-size*1.4f,light);
            for(int i=-2;i<=2;++i) {
                line(cx,floor-size*1.4f,cx+i*size*.55f,cy-size*(1.4f-std::abs(i)*.2f),dim);
                star(cx+i*size*.55f,cy-size*(1.4f-std::abs(i)*.2f),2+rank);
            }
            break;
        default:break;
        }
        // Twinkling eight-point stars, comet tails and tumbling diamond flecks
        // are baked into the cells. Spawn/expiry shrink to zero to avoid pops.
        const int counts[]={0,5,18,30,46};
        const int count=counts[rank];
        for (int s = 0; s < count; ++s) {
            const float phase = std::fmod(t + s * .618033989f, 1.f);
            // Independent angular spacing avoids all stars collapsing into
            // one arc when phase and angle use complementary golden ratios.
            const float angle = tau * (t+s*.414213562f+phase*.25f);
            const float life=std::sin(phase*tau*.5f);
            const float twinkle=.55f+.45f*std::sin(t*tau*2+s*2.4f);
            const float y = floor - 3 - phase * (floor-16);
            const float x = cx + std::sin(angle)*(rank==1?13.f:radius*1.22f);
            const int sparkSize=int(life*(rank==1?1.f+twinkle*2:2.f+rank+twinkle*(rank+1)));
            if(sparkSize<1) continue;
            if(rank>=2 && s%4==0) diamond(x,y,sparkSize*.65f,sparkSize*1.15f);
            else star(x,y,sparkSize);
            if (rank>=2 && s%2==0) {
                line(x,y+sparkSize+1,x+std::sin(angle+.2f)*3,y+sparkSize+3+rank*2,dim);
            }
        }
        const int padding=bloom?bloom_padding:0;
        const int outW=w+2*padding, outH=h+2*padding;
        if(rank>=2) pixels=artwork.reduce();
        if(bloom) pixels=bloom_pixels(pixels,w,h,rank,colour);
        // Split from the bottom. Per-cell native offsets place every row at
        // its original world Y; no overlapping seam and no oversized texture.
        // Bloom is blurred BEFORE splitting so its halo remains continuous.
        for(unsigned part=0;part<parts;++part) {
        const int bottom=int(part)*native_tile_extent;
        const int tileH=std::min(native_tile_extent,outH-bottom);
        append_cell(file,frame*parts+part,pixels,outW,outH,bottom,tileH,padding+ground_anchor_offset-bottom);
        }
    }
    return file;
}
inline EffectSize landing_size(unsigned rank) {
    constexpr int widths[]={0,80,144,192,240},heights[]={0,40,60,80,96};
    return {widths[rank],heights[rank]};
}
// One short, expanding floor burst on landing. Shared by rank/colour, with
// additive soft bands and star tips baked into one native draw per item.
inline std::vector<uint8_t> make_landing_cells(unsigned rank,unsigned colour) {
    const auto size=landing_size(rank);const int w=size.width,h=size.height;
    constexpr uint8_t lights[]={158,111,133,155,98,255};
    const uint8_t light=lights[std::min(colour,5u)];
    std::vector<uint8_t> file(24+frame_count*4);
    auto word=[&](size_t p,uint32_t v) { std::memcpy(file.data()+p,&v,4); };
    word(0,6);word(4,1);word(8,0);word(12,0xeeeeeeee);word(16,1);word(20,frame_count);
    constexpr float tau=6.28318530718f;
    for(unsigned frame=0;frame<frame_count;++frame) {
        EffectCanvas art(w,h);
        const float t=float(frame)/float(frame_count-1);
        const float strength=std::pow(std::max(0.f,std::sin(t*tau*.5f)),.7f);
        const float r=(w*.5f-9)*(.24f+.76f*t),cx=w*.5f,cy=h*.5f;
        const uint8_t white=faded_light(255,strength),bright=faded_light(light,strength);
        const uint8_t soft=faded_light(light,strength*.3f);
        // A bright front with soft shoulders. The last frame is transparent
        // so the one-shot burst can finish without a visible disappearance.
        if(frame && frame+1<frame_count) {
            for(int band=-3;band<=3;++band) for(int a=0;a<720;++a) {
                const float angle=a*tau/720,rr=r+band*.6f;
                art.dot(cx+std::cos(angle)*rr,cy+std::sin(angle)*rr*.28f,
                    band==0?bright:soft);
                if(band==0) art.dot(cx+std::cos(angle)*r*.76f,cy+std::sin(angle)*r*.21f,soft);
            }
            const int count=6+rank*2;
            for(int s=0;s<count;++s) {
                const float angle=s*tau/count+t*.5f;
                const float x=cx+std::cos(angle)*r,y=cy+std::sin(angle)*r*.28f;
                const int length=std::max(1,int((2+rank)*strength));
                for(int i=-length;i<=length;++i) {
                    art.dot(x+i,y,std::abs(i)<=1?white:bright);
                    art.dot(x,y+i,std::abs(i)<=1?white:bright);
                }
            }
        }
        const auto pixels=art.reduce();
        append_cell(file,frame,pixels,w,h,0,h,h/2-13);
    }
    return file;
}
}
