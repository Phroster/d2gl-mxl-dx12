// SPDX-License-Identifier: GPL-3.0-or-later
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "native_loot_cells.h"
#include "native_loot_assets.h"
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
using namespace mxl::native_loot;
using Clock=std::chrono::steady_clock;
static double elapsed(Clock::time_point t) { return std::chrono::duration<double,std::milli>(Clock::now()-t).count(); }
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("usage: native_loot_bake output-directory");
        const auto out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);
        std::ofstream resources(out/"native_loot_assets.rc",std::ios::binary);
        resources<<"// Generated native loot artwork; do not edit.\n#include <winres.h>\n";
        double generationMs=0,compressionMs=0;size_t rawBytes=0,packedBytes=0;unsigned records=0;
        auto save=[&](unsigned id,std::vector<uint8_t>& bytes) {
            const auto start=Clock::now();int length=0;
            auto* compressed=stbi_zlib_compress(bytes.data(),int(bytes.size()),&length,8);
            if(!compressed || length<=0) throw std::runtime_error("compression failed");
            const uint32_t header[]={asset_magic,uint32_t(bytes.size()),asset_checksum(bytes),asset_word(bytes.data()+20)};
            const auto file=out/(std::to_string(id)+".mxa");
            std::ofstream packed(file,std::ios::binary);
            packed.write(reinterpret_cast<const char*>(header),sizeof(header));
            packed.write(reinterpret_cast<const char*>(compressed),length);std::free(compressed);
            if(!packed) throw std::runtime_error("asset write failed");
            resources<<id<<" RCDATA \""<<file.generic_string()<<"\"\n";
            rawBytes+=bytes.size();packedBytes+=length+sizeof(header);++records;compressionMs+=elapsed(start);
        };
        for(unsigned i=1;i<ProfileCount;++i) {
            const auto& p=profiles[i];bool alias=false;
            for(unsigned j=1;j<i;++j) if(p.rank==profiles[j].rank && p.colour==profiles[j].colour && p.style==profiles[j].style) { alias=true;break; }
            if(alias) continue;
            for(bool bloom:{false,true}) {
                if(bloom && p.rank<2) continue;
                const auto start=Clock::now();auto bytes=make_cells(p.rank,p.colour,bloom,p.style);generationMs+=elapsed(start);
                save(effect_resource(i,bloom),bytes);
            }
        }
        for(unsigned rank=1;rank<=4;++rank) for(unsigned colour=0;colour<6;++colour) {
            const auto start=Clock::now();auto bytes=make_landing_cells(rank,colour);generationMs+=elapsed(start);
            save(landing_resource(rank,colour),bytes);
        }
        if(!resources) throw std::runtime_error("resource script write failed");
        std::ofstream stats(out/"bake-timing.json");
        stats<<"{\"generation_ms\":"<<generationMs<<",\"compression_ms\":"<<compressionMs
             <<",\"raw_bytes\":"<<rawBytes<<",\"packed_bytes\":"<<packedBytes<<",\"records\":"<<records<<"}\n";
        std::printf("Baked %u native loot assets: generation %.2f ms, compression %.2f ms; %zu -> %zu bytes\n",records,generationMs,compressionMs,rawBytes,packedBytes);
        return 0;
    } catch(const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
}
