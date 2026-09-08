#include "tile_cache.h"
#include "archive_hash_cache.h"
#include "diagnostics.h"
#include <cstring>
#include <new>

namespace mxl::tiles {
namespace {
Api native{};uintptr_t caller_address=0;
thread_local Cache cache;
thread_local bool initialized=false;
Cache& local_cache(){if(!initialized){cache.begin(native,false);initialized=true;}return cache;}
// Only the DT1 block-loader's verified call site can enter the cache. Keep an
// additional bounded path check so corrupted or unexpected names pass through.
bool tile_name(const char* name,char (&out)[260]) noexcept {
    __try {
        if(!name)return false;
        size_t n=0;for(;n<259 && name[n];++n)out[n]=name[n];
        if(name[n] || n<23)return false;out[n]=0;
        return !_strnicmp(out,"data\\global\\tiles\\",18) && !_stricmp(out+n-4,".dt1") && !strstr(out,"..");
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
void Cache::begin(Api api,bool enabled) {
    const auto error=GetLastError();end();api_=api;
    enabled_=enabled && api.open && api.read && api.close && api.seek && api.size && api.archive;
    stats_={};SetLastError(error);
}
void Cache::release(File& file) {
    if(file.handle)api_.close(file.handle);
    if(file.reader)api_.close(file.reader);
    bytes_-=file.bytes.size();file=File{};
}
Stats Cache::end() {
    const auto error=GetLastError();enabled_=false;
    for(auto& file:files_){
        // An outstanding logical open still belongs to its native caller.
        // Drop our tracking, letting its later close use the native path.
        if(file.active)file.handle=nullptr;
        release(file);
    }
    const auto result=stats_;stats_={};SetLastError(error);return result;
}
Cache::File* Cache::find(void* handle) {if(handle)for(auto& file:files_)if(file.handle==handle)return &file;return nullptr;}
uint32_t Cache::open(const char* path,void** output,bool tile_data_caller) {
    const auto incoming=GetLastError();char snapshot[260]{};
    if(!enabled_ || !tile_data_caller || !output || !tile_name(path,snapshot)){SetLastError(incoming);return api_.open(path,output);}
    File* free=nullptr;
    for(auto& file:files_){
        if(!file.handle){if(!free)free=&file;continue;}
        if(file.active || strcmp(file.path,snapshot))continue;
        SetLastError(incoming);
        if(api_.seek(file.handle,0,nullptr,FILE_BEGIN)==0){
            file.active=true;file.read_seen=false;++file.opens;++stats_.open_hits;*output=file.handle;SetLastError(incoming);return 1;
        }
        release(file);if(!free)free=&file;
    }
    SetLastError(incoming);const auto result=api_.open(path,output);const auto error=GetLastError();
    if(free && result==1 && *output){
        void* archive=nullptr;
        if(api_.archive(*output,&archive) && archive){
            free->handle=*output;free->archive=archive;free->active=true;free->opens=1;
            memcpy(free->path,snapshot,sizeof(snapshot));
        }
    }
    SetLastError(error);return result;
}
bool Cache::prefetch(File& file,uint32_t position,uint32_t requested) {
    if(file.data_disabled)return false;
    if(!file.attempted){
        file.attempted=true;
        uint32_t high=0;const auto size=api_.size(file.handle,&high);
        if(high || size==0 || size>MaxFileBytes || size>MaxBytes-bytes_)return false;
        try{file.bytes.resize(size);}catch(const std::bad_alloc&){return false;}
        bytes_+=size;
        void* temporary=nullptr;void* archive=nullptr;
        const bool opened=api_.open(file.path,&temporary)==1 && temporary;
        if(!opened || !api_.archive(temporary,&archive) || archive!=file.archive){
            if(temporary)api_.close(temporary);
            file.data_disabled=true;++stats_.prefetch_failures;return false;
        }
        file.reader=temporary;
    }
    // Only fetch sectors touched by this request. Native tests rejected an
    // eager whole-file read: it can regress a burst that uses a small subset.
    // A separate real handle keeps failures isolated from the caller's cursor.
    if(!file.reader || position>=file.bytes.size() || requested>=file.bytes.size()-position)return false;
    const auto first=position/PageBytes,last=(position+requested-1)/PageBytes;
    for(size_t page=first;page<=last;){
        if(file.valid[page]){++page;continue;}
        auto stop=page+1;while(stop<=last && !file.valid[stop])++stop;
        const auto offset=uint32_t(page*PageBytes);
        const auto end=uint32_t((stop*PageBytes<file.bytes.size())?stop*PageBytes:file.bytes.size());
        uint32_t got=0;
        if(api_.seek(file.reader,offset,nullptr,FILE_BEGIN)!=offset ||
            api_.read(file.reader,file.bytes.data()+offset,end-offset,&got,0,0,0)!=1 || got!=end-offset){
            file.data_disabled=true;++stats_.prefetch_failures;return false;
        }
        ++stats_.prefetches;stats_.cached_bytes+=got;
        for(;page<stop;++page)file.valid[page]=true;
    }
    return true;
}
uint32_t Cache::read(void* handle,void* buffer,uint32_t requested,uint32_t* completed,uint32_t fifth,uint32_t sixth,uint32_t seventh) {
    const auto incoming=GetLastError();auto* file=enabled_?find(handle):nullptr;
    if(file && file->active && (fifth || sixth || seventh))file->retain=false;
    const bool first=file && file->active && !file->read_seen;
    if(file && file->active)file->read_seen=true;
    if(first && file->retain && buffer && completed && requested){
        // One-off tiles stay native. Repeated block opens can share decoded
        // archive sectors during this producer frame only.
        if(file->opens>=2){
            const auto position=api_.seek(handle,0,nullptr,FILE_CURRENT);
            // Storm clamps seeks to size-1, including FILE_CURRENT queries.
            // The verified block loader seeks once, reads once, then closes.
            // Leave EOF-reaching reads and every subsequent read native.
            if(prefetch(*file,position,requested) &&
                api_.seek(handle,position+requested,nullptr,FILE_BEGIN)==position+requested){
                memcpy(buffer,file->bytes.data()+position,requested);*completed=requested;
                ++stats_.read_hits;stats_.served_bytes+=requested;SetLastError(incoming);return 1;
            }
        }
    }
    SetLastError(incoming);return api_.read(handle,buffer,requested,completed,fifth,sixth,seventh);
}
uint32_t Cache::close(void* handle) {
    const auto incoming=GetLastError();auto* file=enabled_?find(handle):nullptr;
    if(file && file->active && file->retain){file->active=false;SetLastError(incoming);return 1;}
    if(file){file->handle=nullptr;release(*file);}
    SetLastError(incoming);return api_.close(handle);
}
void configure(Api api,uintptr_t tile_open_return){native=api;caller_address=tile_open_return;}
void begin_frame(bool in_game){local_cache().begin(native,in_game && caller_address!=0);}
void end_frame(){
    const auto error=GetLastError();const auto s=cache.end();
    archive_hash::flush_stats();
    if(s.open_hits || s.prefetches || s.prefetch_failures){
        diag::note("tile_cache_open_hits",s.open_hits);diag::note("tile_cache_read_hits",s.read_hits);
        diag::note("tile_cache_prefetched_bytes",s.cached_bytes);diag::note("tile_cache_served_bytes",s.served_bytes);
        if(s.prefetch_failures)diag::note("tile_cache_prefetch_failures",s.prefetch_failures);
    }
    SetLastError(error);
}
uint32_t open(const char* path,void** output,uintptr_t caller){return local_cache().open(path,output,caller==caller_address);}
uint32_t read(void* h,void* b,uint32_t n,uint32_t* c,uint32_t a,uint32_t d,uint32_t e){return local_cache().read(h,b,n,c,a,d,e);}
uint32_t close(void* h){return local_cache().close(h);}
}
