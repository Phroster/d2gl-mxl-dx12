// SPDX-License-Identifier: GPL-3.0-or-later
#include "glide/texture_manager.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

struct Result { unsigned uploads=0, backgroundUploads=0, peak=0; };

// Execute the production allocator and real pixel copies. Loot is drawn before
// the world, changes animation cell every seven display frames, and shares a
// full pool with a stable background. These are controlled workloads, not a
// measurement of the game's actual texture working set or frame times.
static Result replay(unsigned layers,unsigned background,unsigned effects) {
    constexpr unsigned frames=360;
    Result result;
    std::vector<uint8_t> source(128*1024,0x25), atlas(layers*512*512);
    d2gl::g_glide_texture.memory=source.data();
    d2gl::g_glide_texture.hash.clear();
    for(unsigned i=0;i<background;++i) d2gl::g_glide_texture.hash[i]=i+1;
    d2gl::TextureManager cache({{256,uint16_t(layers)}},
        [&](const uint8_t* bytes,const d2gl::SubTextureInfo& slot,uint16_t w,uint16_t h) {
            ++result.uploads;
            for(unsigned y=0;y<h;++y)
                std::memcpy(atlas.data()+slot.tex_num*512*512+(slot.offset.y+y)*512+slot.offset.x,bytes+y*w,w);
        });
    for(unsigned frame=1;frame<=frames;++frame) {
        const auto before=result.uploads;
        for(unsigned i=0;i<effects;++i) {
            const unsigned cell=((frame-1)/7)*effects+i;
            if(!cache.getImmutableSubTextureInfo(cell,256,256,frame,[](uint8_t* bytes) {
                std::fill_n(bytes,256*256,0x31);return true;
            })) throw std::runtime_error("Visible loot did not fit its reserved working set.");
        }
        const auto beforeBackground=result.uploads;
        for(unsigned i=0;i<background;++i)
            if(!cache.getSubTextureInfo(i,256,256,256,frame))
                throw std::runtime_error("Visible background was evicted while still in flight.");
        result.backgroundUploads+=result.uploads-beforeBackground;
        if(frame>1) result.peak=std::max(result.peak,result.uploads-before);
    }
    std::cout << "Controlled " << layers*4 << "-slot replay: uploads=" << result.uploads
        << " background_uploads=" << result.backgroundUploads
        << " peak_uploads_after_warmup=" << result.peak
        << " (360 frames; not live game timing)\n";
    return result;
}

int main() {
    try {
        const auto small=replay(1,3,1);
        const auto gameSized=replay(256,900,124);
        if(small.backgroundUploads!=3 || small.peak!=1 ||
           gameSized.backgroundUploads!=900 || gameSized.peak!=124)
            throw std::runtime_error("Animation pressure re-uploaded a hot background that still fits in the cache.");
        std::cout << "PASS: changing loot cells retain the hot background without bulk cache eviction.\n";
    } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n';return 1; }
}
