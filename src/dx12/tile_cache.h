#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <vector>

namespace mxl::tiles {
using OpenFn=uint32_t(__fastcall*)(const char*,void**);
using ReadFn=uint32_t(__fastcall*)(void*,void*,uint32_t,uint32_t*,uint32_t,uint32_t,uint32_t);
using CloseFn=uint32_t(__fastcall*)(void*);
using SeekFn=uint32_t(__fastcall*)(void*,int32_t,int32_t*,uint32_t);
using SizeFn=uint32_t(__fastcall*)(void*,uint32_t*);
using ArchiveFn=uint32_t(__stdcall*)(void*,void**);
struct Api {OpenFn open=nullptr;ReadFn read=nullptr;CloseFn close=nullptr;SeekFn seek=nullptr;SizeFn size=nullptr;ArchiveFn archive=nullptr;};
struct Stats {uint32_t open_hits=0,read_hits=0,prefetches=0,prefetch_failures=0;uint64_t cached_bytes=0,served_bytes=0;};

// Owns real native handles for one producer frame or synchronous room load. Unknown callers, asynchronous
// reads, loose files, oversized files and capacity misses retain native behavior.
class Cache {
public:
    static constexpr size_t MaxFiles=8,MaxFileBytes=4*1024*1024,MaxBytes=16*1024*1024,PageBytes=4096;
    void begin(Api api,bool enabled);
    Stats end();
    uint32_t open(const char* path,void** output,bool tile_data_caller);
    uint32_t read(void* handle,void* buffer,uint32_t requested,uint32_t* completed,uint32_t fifth,uint32_t sixth,uint32_t seventh);
    uint32_t close(void* handle);
private:
    struct File {
        void* handle=nullptr;void* archive=nullptr;void* reader=nullptr;
        bool active=false,retain=true,attempted=false,read_seen=false,data_disabled=false;
        uint32_t opens=0;char path[260]{};std::vector<uint8_t> bytes;
        std::array<bool,MaxFileBytes/PageBytes> valid{};
    };
    Api api_{};bool enabled_=false;size_t bytes_=0;Stats stats_{};std::array<File,MaxFiles> files_{};
    File* find(void* handle);
    void release(File& file);
    bool prefetch(File& file,uint32_t position,uint32_t requested);
};

void configure(Api api,uintptr_t tile_open_return);
void begin_frame(bool in_game);
void end_frame();
bool configured();
bool begin_load();
void end_load(bool owner);
uint32_t open(const char* path,void** output,uintptr_t caller);
uint32_t read(void* handle,void* buffer,uint32_t requested,uint32_t* completed,uint32_t fifth,uint32_t sixth,uint32_t seventh);
uint32_t close(void* handle);
}
