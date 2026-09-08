#include "tile_cache.h"
#include "diagnostics.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

using namespace mxl::tiles;
namespace {
constexpr DWORD Sentinel=0x12345678,Failure=0x23456789;
struct Data {std::vector<uint8_t> bytes;void* archive=reinterpret_cast<void*>(0x1234);};
struct Handle {Data* data;size_t position=0;bool closed=false;};
std::unordered_map<std::string,Data> files;
std::vector<std::unique_ptr<Handle>> handles;
std::mutex lock;
uint32_t opens=0,reads=0,closes=0,open_fail_after=0,read_fail_size=0,seek_fail_after=0;
uint64_t read_bytes=0;bool loose=false,other_archive=false;
uint32_t extra[3]{};
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
uint32_t fail(){SetLastError(Failure);return 0;}
Handle* cast(void* h){auto* p=static_cast<Handle*>(h);require(p && !p->closed,"Use of a closed native handle.");return p;}
uint32_t __fastcall open_native(const char* name,void** output){
    std::lock_guard guard(lock);++opens;
    if(!output || (open_fail_after && opens>=open_fail_after))return fail();
    auto it=files.find(name);if(it==files.end())return fail();
    handles.emplace_back(std::make_unique<Handle>(Handle{&it->second}));*output=handles.back().get();return 1;
}
uint32_t __fastcall read_native(void* h,void* buffer,uint32_t count,uint32_t* completed,uint32_t a,uint32_t b,uint32_t c){
    std::lock_guard guard(lock);auto* p=cast(h);++reads;extra[0]=a;extra[1]=b;extra[2]=c;
    if(completed)*completed=0;
    if(count==read_fail_size || count>p->data->bytes.size()-p->position)return fail();
    memcpy(buffer,p->data->bytes.data()+p->position,count);p->position+=count;read_bytes+=count;
    if(completed)*completed=count;return 1;
}
uint32_t __fastcall close_native(void* h){std::lock_guard guard(lock);cast(h)->closed=true;++closes;return 1;}
uint32_t __fastcall seek_native(void* h,int32_t distance,int32_t* high,uint32_t origin){
    std::lock_guard guard(lock);auto* p=cast(h);
    if(high && *high){fail();return UINT32_MAX;}
    if(seek_fail_after && --seek_fail_after==0){fail();return UINT32_MAX;}
    const int64_t base=origin==FILE_CURRENT?p->position:origin==FILE_END?p->data->bytes.size():0;
    p->position=size_t(std::clamp(base+distance,int64_t(0),int64_t(p->data->bytes.size()-1)));
    return uint32_t(p->position);
}
uint32_t __fastcall size_native(void* h,uint32_t* high){std::lock_guard guard(lock);if(high)*high=0;return uint32_t(cast(h)->data->bytes.size());}
uint32_t __stdcall archive_native(void* h,void** archive){std::lock_guard guard(lock);*archive=loose?nullptr:other_archive?reinterpret_cast<void*>(0x5678):cast(h)->data->archive;return 1;}
const Api api{open_native,read_native,close_native,seek_native,size_native,archive_native};
const char* path="data\\global\\tiles\\test\\tile.dt1";
void reset(){
    for(const auto& h:handles)require(h->closed,"Native handle leaked.");
    handles.clear();files.clear();opens=reads=closes=open_fail_after=read_fail_size=seek_fail_after=0;read_bytes=0;loose=other_archive=false;
}
void add(const std::string& name,size_t size){auto& d=files[name];d.bytes.resize(size);for(size_t i=0;i<size;++i)d.bytes[i]=uint8_t((i*17+i/11)%251);}
void slice(Cache& cache,const char* name,size_t offset,size_t length,bool eligible=true){
    void* h=nullptr;SetLastError(Sentinel);require(cache.open(name,&h,eligible)==1,"Open failed.");require(GetLastError()==Sentinel,"Open changed LastError.");
    require(seek_native(h,int32_t(offset),nullptr,FILE_BEGIN)==offset,"Initial seek.");
    std::vector<uint8_t> bytes(length);uint32_t got=0;SetLastError(Sentinel);
    require(cache.read(h,bytes.data(),uint32_t(length),&got,0,0,0)==1 && got==length,"Read result/count.");
    require(GetLastError()==Sentinel,"Read changed LastError.");
    require(std::equal(bytes.begin(),bytes.end(),files[name].bytes.begin()+offset),"Read changed bytes.");
    require(cast(h)->position==offset+length,"Read changed native position.");
    require(cache.close(h)==1 && GetLastError()==Sentinel,"Close result/error.");
}
}
int main(){try{
    require(!mxl::diag::enabled(),"Test must run with recording off.");
    Cache cache;
    add(path,65536);cache.begin(api,true);
    slice(cache,path,99,900);slice(cache,path,199,700);slice(cache,path,399,200);
    auto stats=cache.end();
    require(stats.open_hits==2 && stats.read_hits==2 && stats.cached_bytes==Cache::PageBytes,"Repeated request did not reuse one requested sector.");
    require(opens==2 && closes==2 && read_bytes==900+Cache::PageBytes,"Unexpected prefetch amplification/handle lifecycle.");
    reset();

    // A caller can continue reading after the cached first read. EOF must not
    // be converted to size-1 by an internal FILE_CURRENT seek.
    add(path,8192);cache.begin(api,true);slice(cache,path,2,4);
    void* h=nullptr;cache.open(path,&h,true);seek_native(h,8190,nullptr,FILE_BEGIN);
    uint8_t bytes[16]{};uint32_t got=0;
    require(cache.read(h,bytes,2,&got,0,0,0)==1 && cast(h)->position==8192,"EOF-reaching read must remain native.");
    require(cache.read(h,bytes,1,&got,0,0,0)==0 && got==0 && GetLastError()==Failure && cast(h)->position==8192,"Read after EOF semantics changed.");
    cache.close(h);cache.end();reset();

    // Native flags and all seven arguments survive the unsupported-read path.
    add(path,8192);cache.begin(api,true);slice(cache,path,2,4);cache.open(path,&h,true);
    cache.read(h,bytes,3,&got,0x155,0x266,0x377);cache.close(h);
    require(extra[0]==0x155 && extra[1]==0x266 && extra[2]==0x377,"Read flags changed.");
    require(closes==1,"An asynchronous/unknown read retained its native handle.");
    cache.end();reset();

    // Failed prefetches do not touch the original read cursor or output.
    for(unsigned failure=0;failure<4;++failure){
        add(path,8192);cache.begin(api,true);slice(cache,path,40,20);
        if(failure==0)open_fail_after=opens+1;
        if(failure==1)read_fail_size=Cache::PageBytes;
        if(failure==2)other_archive=true;
        // Hit reset, caller seek, caller current query, then reader seek.
        if(failure==3)seek_fail_after=4;
        slice(cache,path,40,20);stats=cache.end();
        require(stats.prefetch_failures==1 && stats.read_hits==0,"Failure did not take the native read path.");
        reset();
    }

    // Capacity and byte budgets leave excess requests native; they do not evict
    // active handles. Per-frame end frees the entire bounded cache.
    for(size_t i=0;i<Cache::MaxFiles+1;++i)add("data\\global\\tiles\\test\\"+std::to_string(i)+".dt1",Cache::MaxFileBytes);
    cache.begin(api,true);
    for(size_t i=0;i<Cache::MaxFiles+1;++i){auto name="data\\global\\tiles\\test\\"+std::to_string(i)+".dt1";slice(cache,name.c_str(),99,12);slice(cache,name.c_str(),99,12);}
    stats=cache.end();require(stats.open_hits==Cache::MaxFiles && stats.read_hits==Cache::MaxBytes/Cache::MaxFileBytes,"Capacity/byte bound failed.");reset();

    add(path,Cache::MaxFileBytes+1);cache.begin(api,true);slice(cache,path,1,2);slice(cache,path,1,2);stats=cache.end();require(stats.read_hits==0,"Oversized file cached.");reset();
    add(path,8192);loose=true;cache.begin(api,true);slice(cache,path,1,2);slice(cache,path,1,2);stats=cache.end();require(stats.open_hits==0 && opens==closes,"Loose file cached.");reset();
    add(path,8192);cache.begin(api,true);slice(cache,path,1,2,false);slice(cache,path,1,2,false);stats=cache.end();require(stats.open_hits==0 && opens==closes,"Unknown caller cached.");reset();
    add(path,8192);cache.begin(api,false);slice(cache,path,1,2);slice(cache,path,1,2);stats=cache.end();require(stats.open_hits==0 && opens==closes,"Disabled cache changed behavior.");reset();

    // An open surviving a frame boundary remains owned by its caller.
    add(path,8192);cache.begin(api,true);cache.open(path,&h,true);cache.end();require(!cast(h)->closed,"Active handle closed at frame end.");
    cache.read(h,bytes,3,&got,0,0,0);cache.close(h);reset();

    // Nested same-name opens must have independent native cursors.
    add(path,8192);cache.begin(api,true);void* second=nullptr;cache.open(path,&h,true);cache.open(path,&second,true);
    require(h!=second,"Nested opens share a cursor.");cache.close(second);cache.close(h);cache.end();reset();

    add(path,8192);configure(api,0x1234);std::atomic<unsigned> failures=0;
    auto worker=[&](){try{begin_frame(true);void* local=nullptr;for(unsigned i=0;i<3;++i){mxl::tiles::open(path,&local,0x1234);uint8_t b[8]{};uint32_t n=0;mxl::tiles::read(local,b,8,&n,0,0,0);require(n==8,"Thread read count.");mxl::tiles::close(local);}end_frame();}catch(...){++failures;}};
    std::thread first(worker),second_thread(worker);first.join();second_thread.join();require(failures==0,"Thread-local cache failed.");reset();
    std::cout<<"PASS: native bytes/cursors/errors, bounded sector demand, failures, EOF, flags, capacity, frame lifetime and thread isolation; recording disabled.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
