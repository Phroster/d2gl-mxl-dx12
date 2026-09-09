// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace mxl::native_loot {
// Only renderer-owned, process-lifetime DC6 cells enter this scope. The native
// drawer still supplies geometry, clipping, blending and world draw order.
// Its reusable TMU address is not an identity for these immutable animations.
struct SpritePixels {
    uintptr_t identity=0;
    const uint8_t* rle=nullptr;
    uint32_t length=0,width=0,height=0;

    bool decode(uint8_t* output,unsigned textureWidth,unsigned textureHeight) const {
        if(!rle || !output || !width || !height || width>textureWidth || height>textureHeight
            || textureWidth>256 || textureHeight>256) return false;
        std::fill_n(output,size_t(textureWidth)*textureHeight,0);
        size_t at=0;
        // DC6 rows run upwards from the native anchor. The hardware drawer
        // aligns them to the bottom of its padded power-of-two texture.
        for(unsigned row=0;row<height;++row) {
            unsigned x=0;
            for(;;) {
                if(at==length) return false;
                const auto code=rle[at++];
                if(code==0x80) break;
                const unsigned count=code&0x7f;
                if(!count || count>width-x) return false;
                if(!(code&0x80)) {
                    if(count>length-at) return false;
                    std::copy_n(rle+at,count,output+(textureHeight-1-row)*textureWidth+x);
                    at+=count;
                }
                x+=count;
            }
        }
        return at==length;
    }
};

inline thread_local const SpritePixels* currentSpritePixels=nullptr;
class SpriteScope {
    const SpritePixels* previous;
public:
    explicit SpriteScope(const SpritePixels& pixels):previous(currentSpritePixels) { currentSpritePixels=&pixels; }
    ~SpriteScope() { currentSpritePixels=previous; }
    SpriteScope(const SpriteScope&)=delete;
    SpriteScope& operator=(const SpriteScope&)=delete;
};
}
