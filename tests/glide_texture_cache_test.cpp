// SPDX-License-Identifier: GPL-3.0-or-later
#include "glide/texture_manager.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <tuple>

using namespace d2gl;
static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static auto id(const SubTextureInfo& slot) { return std::tuple(slot.tex_num, slot.offset.x, slot.offset.y); }

int main() {
    try {
        std::vector<uint8_t> bytes(128 * 1024);
        g_glide_texture.memory = bytes.data();
        unsigned uploads = 0;
        // One 512x512 layer holds four 256x256 slots. Exercise the production
        // allocator with a deliberately small pool so pressure is reproducible.
        TextureManager cache({{256, 1}}, [&](const uint8_t*, const SubTextureInfo&, uint16_t, uint16_t) { ++uploads; });
        for (unsigned address = 0; address < 8; ++address) g_glide_texture.hash[address] = address + 100;
        std::array<SubTextureInfo, 4> live{};
        for (unsigned address = 0; address < 4; ++address) {
            auto* slot = cache.getSubTextureInfo(address, 256, 256, 256, 1);
            require(slot != nullptr, "initial allocation failed"); live[address] = *slot;
        }
        require(uploads == 4, "initial upload count");
        require(!cache.getSubTextureInfo(4, 256, 256, 256, 1), "reused a slot still referenced by this frame");
        for (unsigned address = 0; address < 4; ++address)
            require(id(*cache.getSubTextureInfo(address, 256, 256, 256, 1)) == id(live[address]), "current frame binding changed");
        // A new address next frame must reclaim inactive entries instead of
        // failing and causing grTexSource to keep the preceding sprite bound.
        auto* next = cache.getSubTextureInfo(4, 256, 256, 256, 2);
        require(next != nullptr, "stale addresses exhausted the sprite atlas on the next frame");
        require(uploads == 5, "reclaimed slot was not uploaded");
        cache.clearCache();
        require(cache.getUsage(256) == 0, "clear did not release all slots");
        // Multiple animation contents at one virtual address remain distinct
        // until this frame has finished. The renderer batches uploads first.
        for (unsigned version = 0; version < 4; ++version) {
            g_glide_texture.hash[0] = version + 200;
            auto* slot = cache.getSubTextureInfo(0, 256, 256, 256, 3);
            require(slot != nullptr, "animated texture version missing");
            live[version] = *slot;
            for (unsigned old = 0; old < version; ++old)
                require(id(live[old]) != id(*slot), "animation overwrote a queued sprite");
        }
        g_glide_texture.hash[0] = 300;
        require(!cache.getSubTextureInfo(0, 256, 256, 256, 3), "animation evicted an in-flight version");
        require(cache.getSubTextureInfo(0, 256, 256, 256, 4) != nullptr, "old animation versions not released");
        cache.clearCache();
        for (unsigned address = 0; address < 4; ++address)
            require(cache.getSubTextureInfo(address, 256, 256, 256, 5) != nullptr, "refill failed");
        const auto pinned = *cache.getSubTextureInfo(0, 256, 256, 256, 6);
        for (unsigned address = 4; address < 7; ++address) {
            auto* slot = cache.getSubTextureInfo(address, 256, 256, 256, 6);
            require(slot && id(*slot) != id(pinned), "pressure reclaimed a visible sprite");
        }
        require(!cache.getSubTextureInfo(7, 256, 256, 256, 6), "full active frame did not reject overflow");
        // Different scenes continually introduce addresses. Retired addresses
        // must not accumulate until the pool becomes permanently unusable.
        for (unsigned frame = 7; frame < 1007; ++frame) {
            for (unsigned item = 0; item < 4; ++item) {
                const unsigned address = frame * 4 + item;
                g_glide_texture.hash[address] = address;
                auto* slot = cache.getSubTextureInfo(address, 256, 256, 256, frame);
                require(slot != nullptr, "scene churn permanently exhausted the cache");
            }
        }
        require(cache.stats().reclaimed_slots > 3000, "pressure diagnostics did not count reclamation");
        require(!cache.getSubTextureInfo(999999, 256, 256, 256, 1008), "missing source was accepted");
        require(cache.stats().missing_source == 1, "missing source diagnostics");
        // Different native cell shapes can have identical packed bytes. In
        // particular a transparent 256x128 landing frame and a 128x256 cell
        // hash identically. Reusing only the first rectangle leaves old art
        // visible in the lower part of the second rectangle.
        cache.clearCache();
        std::vector<uint8_t> atlas(512*512,0xa7);
        TextureManager shapes({{256,1}},[&](const uint8_t* pixels,const SubTextureInfo& slot,uint16_t w,uint16_t h) {
            for(unsigned y=0;y<h;++y)std::copy_n(pixels+y*w,w,atlas.data()+(slot.offset.y+y)*512+slot.offset.x);
        });
        std::fill(bytes.begin(),bytes.end(),0);g_glide_texture.hash[0]=123;
        const auto wide=*shapes.getSubTextureInfo(0,256,256,128,1);
        const auto tall=*shapes.getSubTextureInfo(0,256,128,256,1);
        for(unsigned y=0;y<256;++y)for(unsigned x=0;x<128;++x)
            require(atlas[(tall.offset.y+y)*512+tall.offset.x+x]==0,"changed sprite shape exposed stale atlas pixels");
        require(id(wide)!=id(tall),"different sprite shapes shared a queued texture slot");
        std::cout << "PASS: production sprite cache pressure, frame pinning, animation versions and reset.\n";
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
