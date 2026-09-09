// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include "native_loot_rules.h"

namespace mxl::native_loot {
constexpr uint32_t asset_magic=0x3141584d; // MXA1, followed by size, checksum, cells.
constexpr uint32_t max_asset_bytes=8*1024*1024;
constexpr unsigned effect_resource(unsigned profile,bool bloom) { return 8000+profile*2+unsigned(bloom); }
constexpr unsigned landing_resource(unsigned rank,unsigned colour) { return 9000+(rank-1)*6+colour; }
inline uint32_t asset_word(const uint8_t* p) { uint32_t v;std::memcpy(&v,p,4);return v; }
inline uint32_t asset_checksum(std::span<const uint8_t> data) {
    uint32_t hash=2166136261u;
    for(auto byte:data) hash=(hash^byte)*16777619u;
    return hash;
}
// The renderer reads only embedded build assets. Bound allocations and check
// the full decoded payload before passing writable bytes to native D2CMP.
template<class Inflate>
bool unpack_asset(std::span<const uint8_t> packed,unsigned expectedCells,std::vector<uint8_t>& output,Inflate inflate) {
    output.clear();
    if(packed.size()<17 || packed.size()>max_asset_bytes+16 || asset_word(packed.data())!=asset_magic) return false;
    const auto size=asset_word(packed.data()+4),checksum=asset_word(packed.data()+8),cells=asset_word(packed.data()+12);
    if(!cells || cells>96 || cells!=expectedCells || size<24+cells*4 || size>max_asset_bytes) return false;
    output.resize(size);
    if(inflate(reinterpret_cast<char*>(output.data()),int(size),reinterpret_cast<const char*>(packed.data()+16),int(packed.size()-16))!=int(size)
        || asset_checksum(output)!=checksum || asset_word(output.data())!=6
        || asset_word(output.data()+4)!=1 || asset_word(output.data()+20)!=cells) { output.clear();return false; }
    for(unsigned i=0;i<cells;++i) {
        const auto at=asset_word(output.data()+24+i*4);
        if(at<24+cells*4 || at>size-35) { output.clear();return false; }
        const auto width=asset_word(output.data()+at+4),height=asset_word(output.data()+at+8),length=asset_word(output.data()+at+28);
        if(!width || width>256 || !height || height>256 || length>size-at-35) { output.clear();return false; }
    }
    return true;
}
}
