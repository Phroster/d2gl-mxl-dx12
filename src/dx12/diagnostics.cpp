#include "diagnostics.h"
#include "input_profile.h"
#include "reveal_probe.h"
#include "asset_probe.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <share.h>

namespace mxl::diag {
namespace {
constexpr size_t Metrics=size_t(Metric::Count), Counts=size_t(Count::Count), AudioOps=size_t(Audio::Count), Capacity=8192;
const char* metric_names[]={"render_ms","input_wait_ms","gpu_fence_wait_ms","present_call_ms","latency_wait_ms","submit_ms","pipeline_ms","bindings_ms","index_scan_ms","upload_ms","allocation_ms","producer_build_ms"};
const char* count_names[]={"draws","indices","texture_bytes","buffer_bytes","spill_bytes","new_pipelines","binding_misses","barriers","minimap","width","height","game_screen","new_textures"};
const char* audio_names[]={"factory","create_buffer","duplicate_buffer","play","stop","lock","unlock","volume","pan","frequency","cursor","restore","parameters_3d","position_3d","commit_3d"};
static_assert(std::size(metric_names)==Metrics && std::size(count_names)==Counts && std::size(audio_names)==AudioOps);
struct Record {
    const char* kind="note"; uint64_t id=0,at=0; uint32_t tid=0;
    double duration=0,interval=0; bool focused=false,slow=false;
    std::array<double,Metrics> ms{}; std::array<uint64_t,Counts> counts{};
    char detail[96]{}; int64_t value=0;
};
struct AudioTotals { std::atomic<uint64_t> calls{0},elapsed{0},maximum{0},errors{0}; };
struct State {
    std::atomic<bool> active{false},quitting{false}; bool audio=true,assets=false,testing=false;
    std::atomic<uint64_t> dropped{0}; HWND window=nullptr; HANDLE worker=nullptr,owner=nullptr;
    LARGE_INTEGER frequency{}; uint64_t started=0; double threshold=10.0,audio_threshold=.5;
    std::wstring directory; SRWLOCK lock=SRWLOCK_INIT;
    std::array<Record,Capacity> queue{}; size_t read=0,write=0,size=0;
    std::array<AudioTotals,AudioOps> audio_totals{};
};
State& state() { static State* s=new State;return *s; }
thread_local Record frame;
thread_local bool frame_active=false;
thread_local uint64_t previous_end=0;
thread_local bool previous_focused=false;
thread_local uint32_t previous_screen=0;
void put(const Record& record) noexcept {
    auto& s=state();
    if(!s.active.load(std::memory_order_relaxed)) return;
    if(!TryAcquireSRWLockExclusive(&s.lock)){++s.dropped;return;}
    if(s.size==Capacity) ++s.dropped;
    else {s.queue[s.write]=record;s.write=(s.write+1)%Capacity;++s.size;}
    ReleaseSRWLockExclusive(&s.lock);
}
void header(FILE* file) {
    fputs("type,frame_id,session_ms,thread_id,duration_ms,interval_ms,focused,slow",file);
    for(auto name:metric_names) fprintf(file,",%s",name);
    for(auto name:count_names) fprintf(file,",%s",name);
    fputs(",detail,value\n",file);
}
void write_record(FILE* file,const Record& r) {
    fprintf(file,"%s,%llu,%.6f,%u,%.6f,%.6f,%u,%u",r.kind,
        (unsigned long long)r.id,milliseconds(r.at-state().started),r.tid,r.duration,r.interval,unsigned(r.focused),unsigned(r.slow));
    for(auto ms:r.ms) fprintf(file,",%.6f",ms);
    for(auto value:r.counts) fprintf(file,",%llu",(unsigned long long)value);
    fprintf(file,",%s,%lld\n",r.detail,(long long)r.value);
}
void write_input(FILE* file,const Record& r) {
    const auto flags=uint32_t(r.counts[8]);
    const auto value=[&](size_t index,uint32_t flag)->long long {return flags&flag?static_cast<long long>(r.counts[index]):-1ll;};
    fprintf(file,"%llu,%.6f,%u,%.6f,%.6f,%.6f,%.6f,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%u,%s,%lld\n",
        (unsigned long long)r.id,milliseconds(r.at-state().started),r.tid,r.duration,r.ms[0],r.ms[1],r.ms[2],
        value(0,ThreadCycles),value(1,ProcessIo),value(2,ProcessIo),value(3,ProcessIo),value(4,ProcessIo),
        value(5,ProcessIo),value(6,ProcessIo),value(7,ProcessFaults),flags,r.detail,(long long)r.value);
}
DWORD WINAPI writer(void*) {
    auto& s=state();SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
    FILE* file=nullptr;FILE* inputs=nullptr;FILE* reveals=nullptr;FILE* assets=nullptr;
    uint32_t part=0,input_count=0,reveal_count=0,asset_count=0;uint64_t last_summary=ticks(),written=0,slow=0;
    auto open=[&](){
        const auto name=s.directory+L"\\events-"+std::to_wstring(part%3)+L".csv";
        file=_wfsopen(name.c_str(),L"wb",_SH_DENYNO);
        if(!file)return false;
        setvbuf(file,nullptr,_IOFBF,256*1024);header(file);return true;
    };
    if(!open()){s.active=false;return 1;}
    std::array<Record,64> batch;
    for(;;) {
        size_t count=0;
        AcquireSRWLockExclusive(&s.lock);
        while(s.size && count<batch.size()) {batch[count++]=s.queue[s.read];s.read=(s.read+1)%Capacity;--s.size;}
        ReleaseSRWLockExclusive(&s.lock);
        for(size_t i=0;i<count;++i){
            if(!strcmp(batch[i].kind,"asset")) {
                if(!assets && asset_count==0){
                    assets=_wfsopen((s.directory+L"\\assets.csv").c_str(),L"wb",_SH_DENYNO);
                    if(assets)fputs("operation,source,handle,session_ms,thread_id,duration_ms,path,path_status,requested_bytes,completed_bytes,output_valid,result\n",assets);
                }
                if(assets && asset_count<131072){
                    const auto& r=batch[i];const char* operations[]={"open","read","close"};
                    fprintf(assets,"%s,%s,%llu,%.6f,%u,%.6f,\"",operations[r.counts[0]],r.counts[1]?"D2Sound":"D2CMP",
                        (unsigned long long)r.id,milliseconds(r.at-s.started),r.tid,r.duration);
                    for(const char* c=r.detail;*c;++c){if(*c=='\"')fputc('\"',assets);fputc(*c,assets);}
                    fprintf(assets,"\",%llu,%llu,%llu,%llu,%lld\n",(unsigned long long)r.counts[4],
                        (unsigned long long)r.counts[2],(unsigned long long)r.counts[3],(unsigned long long)r.counts[5],(long long)r.value);
                    ++asset_count;
                }else ++s.dropped;
            } else if(!strcmp(batch[i].kind,"reveal")) {
                if(!reveals && reveal_count==0){
                    reveals=_wfsopen((s.directory+L"\\reveal.csv").c_str(),L"wb",_SH_DENYNO);
                    if(reveals)fputs("trace_id,phase,session_ms,thread_id,duration_ms,act,level,room_x,room_y,resident_before\n",reveals);
                }
                if(reveals && reveal_count<262144){const auto& r=batch[i];fprintf(reveals,"%llu,%s,%.6f,%u,%.6f,%d,%d,%d,%d,%u\n",
                    (unsigned long long)r.id,r.detail,milliseconds(r.at-s.started),r.tid,r.duration,
                    int32_t(r.counts[0]),int32_t(r.counts[1]),int32_t(r.counts[2]),int32_t(r.counts[3]),unsigned(r.counts[4]));++reveal_count;}
                else ++s.dropped;
            } else if(!strcmp(batch[i].kind,"key_T_profile")) {
                if(!inputs && input_count==0) {
                    inputs=_wfsopen((s.directory+L"\\input.csv").c_str(),L"wb",_SH_DENYNO);
                    if(inputs)fputs("input_id,session_ms,thread_id,wall_ms,user_cpu_ms,kernel_cpu_ms,wall_minus_cpu_ms,thread_cycles,process_read_bytes,process_read_ops,process_write_bytes,process_write_ops,process_other_bytes,process_other_ops,process_page_faults,valid_flags,procedure_module,procedure_offset\n",inputs);
                }
                if(inputs && input_count<4096){write_input(inputs,batch[i]);++input_count;}
                else ++s.dropped;
            } else write_record(file,batch[i]);
            ++written;if(batch[i].slow)++slow;
        }
        if(_ftelli64(file)>32ll*1024*1024){fclose(file);file=nullptr;++part;if(!open()){s.active=false;return 2;}}
        const auto now=ticks();
        if(milliseconds(now-last_summary)>=1000 || s.quitting.load()) {
            for(size_t i=0;i<AudioOps;++i) {
                auto& a=s.audio_totals[i];auto calls=a.calls.exchange(0);auto elapsed=a.elapsed.exchange(0);
                auto maximum=a.maximum.exchange(0),errors=a.errors.exchange(0);
                if(calls || errors || elapsed || maximum) {
                    Record r;r.kind="audio_summary";r.at=now;r.duration=milliseconds(elapsed);r.interval=milliseconds(maximum);
                    r.counts[0]=calls;r.counts[1]=errors;strcpy_s(r.detail,audio_names[i]);write_record(file,r);
                }
            }
            Record status;status.kind="logger";status.at=now;status.value=s.dropped.load();strcpy_s(status.detail,"dropped_records");write_record(file,status);
            fflush(file);if(inputs)fflush(inputs);if(reveals)fflush(reveals);if(assets)fflush(assets);last_summary=now;
            FILE* out=nullptr;const auto path=s.directory+L"\\status.txt";
            if(!_wfopen_s(&out,path.c_str(),L"wb") && out) {
                fprintf(out,"MXL Smooth Motion DX12 1.0 diagnostics\nstate=%s\nrecords=%llu\nslow_frames=%llu\ndropped_records=%llu\n",
                    s.quitting?"stopped":"recording",(unsigned long long)written,(unsigned long long)slow,(unsigned long long)s.dropped.load());fclose(out);
            }
            if(GetFileAttributesW((s.directory+L"\\STOP").c_str())!=INVALID_FILE_ATTRIBUTES) {
                s.active=false;s.quitting=true;
            }
        }
        if(s.quitting && count==0) break;
        if(count==0) Sleep(50);
    }
    if(inputs){fflush(inputs);fclose(inputs);}if(reveals){fflush(reveals);fclose(reveals);}
    if(assets){fflush(assets);fclose(assets);}fflush(file);fclose(file);return 0;
}
}
uint64_t ticks() noexcept {LARGE_INTEGER t;QueryPerformanceCounter(&t);return uint64_t(t.QuadPart);}
double milliseconds(uint64_t elapsed) noexcept {return state().frequency.QuadPart?double(elapsed)*1000.0/state().frequency.QuadPart:0;}
bool enabled() noexcept {return state().active.load(std::memory_order_relaxed);}
bool audio_enabled() noexcept {return enabled()&&state().audio;}
bool assets_enabled() noexcept {return enabled()&&state().assets;}
bool start(HWND window,const std::wstring& test_directory) {
    auto& s=state();if(s.worker)return enabled();
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    const auto root=std::filesystem::path(executable).parent_path();
    if(test_directory.empty() && _wcsicmp(std::filesystem::path(executable).filename().c_str(),L"Game.exe")) return false;
    const auto ini=(root/L"mxl-diagnostics.ini").wstring();
    if(test_directory.empty() && !GetPrivateProfileIntW(L"Diagnostics",L"enabled",0,ini.c_str()))return false;
    const auto owner_name=L"Local\\MXLPrivateDiagnostics-"+std::to_wstring(GetCurrentProcessId());
    s.owner=CreateMutexW(nullptr,FALSE,owner_name.c_str());
    if(!s.owner)return false;
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(s.owner);s.owner=nullptr;return false;}
    s.audio=GetPrivateProfileIntW(L"Diagnostics",L"audio",1,ini.c_str())!=0;
    s.assets=GetPrivateProfileIntW(L"Diagnostics",L"assets",0,ini.c_str())!=0;
    s.threshold=std::clamp(GetPrivateProfileIntW(L"Diagnostics",L"slow_frame_ms",10,ini.c_str()),5u,1000u);
    QueryPerformanceFrequency(&s.frequency);s.started=ticks();s.window=window;s.testing=!test_directory.empty();
    SYSTEMTIME utc{},local{};GetSystemTime(&utc);GetLocalTime(&local);wchar_t session[80];
    swprintf_s(session,L"%04u%02u%02u-%02u%02u%02u-pid%lu",local.wYear,local.wMonth,local.wDay,local.wHour,local.wMinute,local.wSecond,GetCurrentProcessId());
    s.directory=test_directory.empty()?(root/L"mxl-diagnostics"/session).wstring():test_directory;
    std::error_code error;std::filesystem::create_directories(s.directory,error);if(error)return false;
    FILE* file=nullptr;
    if(!_wfopen_s(&file,(s.directory+L"\\session.txt").c_str(),L"wb") && file) {
        fprintf(file,"MXL Smooth Motion DX12 1.0 diagnostics\npid=%lu\nqpc_frequency=%lld\nqpc_start=%llu\n",
            GetCurrentProcessId(),(long long)s.frequency.QuadPart,(unsigned long long)s.started);
        fprintf(file,"utc_start=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\nlocal_start=%04u-%02u-%02u %02u:%02u:%02u.%03u\n",
            utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds,
            local.wYear,local.wMonth,local.wDay,local.wHour,local.wMinute,local.wSecond,local.wMilliseconds);
        fprintf(file,"slow_frame_ms=%.2f\naudio=%u\nassets=%u\nGPU timestamps cover our DX12 command lists, not ReShade's separate submissions.\n",
            s.threshold,unsigned(s.audio),unsigned(s.assets));
        fputs("Audio summaries: duration_ms=sum of completed call wall times; interval_ms=largest call; draws=calls; indices=failed calls.\n"
              "input.csv: T handler thread CPU/cycles plus process-wide I/O and page-fault deltas.\n"
              "Wall minus CPU includes waits and descheduling, not just explicit Sleep. CPU accounting has finite granularity.\n"
              "I/O counts include cached/device I/O and other threads; they are not physical disk bytes. Fault counts include soft faults.\n"
              "Input valid_flags: 1=thread CPU, 2=process I/O, 4=process faults, 8=thread cycles. -1 means unavailable.\n"
              "Procedure module/offset identifies the forwarded window-procedure entrypoint, not a sampled inner hotspot.\n"
              "At most 4096 detailed T profiles per session. Only T key-down triggers these extra counters.\n"
              "reveal.csv: guarded MXL act/level/room and D2Common generate/load/unload durations. These scopes are nested, not additive.\n"
              "Reveal depth probe: preset generation/layout, preset preparation, DT1 loading, tile grids, level lookup and automap layer selection.\n"
              "Depth scopes forward the original game functions without deferring, skipping or replacing reveal work. Missing rows may reflect dropped records.\n"
              "At most 262144 reveal phase rows per session. No work is deferred or skipped by this probe.\n"
              "assets.csv: opt-in D2CMP/D2Sound archive open/read/close wall times through verified Fog imports. Paths are bounded to 95 bytes.\n"
              "Asset path_status: 0=complete, 1=truncated, 2=unreadable, 3=no path for this operation. output_valid refers to open handle or read byte output.\n"
              "Asset handles can be reused; match successful open/close lifetimes before assigning read paths. Missing opens leave unknown paths.\n"
              "Archive read time includes Storm processing, not just physical disk access; this does not time later sprite decoding or sound mixing. Async reads time submission only.\n"
              "At most 131072 asset records per session. Rows time logical requests, including any guarded DT1 sector caching; internal cache reads are included in the parent duration.\n"
              "tile_cache_* and archive_hash_* notes report optional, independently guarded caches. Hash reuse is limited to one native CMP file open and never caches archive selection.\n"
              "Frame render_ms includes nested scopes. Do not add them together.\n"
              "No per-frame disk writes on game/render/audio threads; the queue can drop samples instead of blocking.\n"
              "At most three 32 MiB CSV files per session. Create an empty STOP file here to stop recording.\n",file);fclose(file);
    }
    if(test_directory.empty()) {HMODULE self=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&start),&self);}
    s.active=true;s.worker=CreateThread(nullptr,0,writer,nullptr,0,nullptr);
    if(!s.worker){s.active=false;return false;}note("logger_started");return true;
}
uint64_t current_frame() noexcept {return frame_active?frame.id:0;}
void begin_frame(uint64_t id,double input_wait,uint32_t width,uint32_t height,bool minimap,uint32_t screen) noexcept {
    frame_active=enabled();if(!frame_active)return;
    frame={};frame.kind="frame";frame.id=id;frame.at=ticks();frame.tid=GetCurrentThreadId();
    frame.ms[size_t(Metric::InputWait)]=input_wait;
    frame.counts[size_t(Count::Width)]=width;frame.counts[size_t(Count::Height)]=height;frame.counts[size_t(Count::Minimap)]=minimap;
    frame.counts[size_t(Count::GameScreen)]=screen;
}
void end_frame() noexcept {
    if(!frame_active)return;const auto end=ticks();frame.duration=milliseconds(end-frame.at);
    frame.interval=previous_end?milliseconds(end-previous_end):0;previous_end=end;
    frame.focused=state().testing || GetForegroundWindow()==state().window;
    const auto screen=uint32_t(frame.counts[size_t(Count::GameScreen)]);
    frame.slow=frame.focused && previous_focused && screen==1 && previous_screen==1 && frame.interval>=state().threshold && milliseconds(end-state().started)>2000;
    previous_focused=frame.focused;previous_screen=screen;
    put(frame);frame_active=false;
}
void add(Metric metric,uint64_t elapsed) noexcept {if(frame_active)frame.ms[size_t(metric)]+=milliseconds(elapsed);}
void count(Count metric,uint64_t amount) noexcept {if(frame_active)frame.counts[size_t(metric)]+=amount;}
void producer(uint64_t id,uint64_t ready,uint64_t returned,double interval,uint32_t vertices,double build_ms) noexcept {
    if(!enabled())return;Record r;r.kind="producer";r.id=id;r.at=ready;r.tid=GetCurrentThreadId();
    r.duration=milliseconds(returned-ready);r.interval=interval;r.counts[0]=vertices;r.ms[size_t(Metric::ProducerBuild)]=build_ms;put(r);
}
void gpu_batch(uint64_t id,double duration) noexcept {if(!enabled())return;Record r;r.kind="gpu";r.id=id;r.at=ticks();r.duration=duration;r.tid=GetCurrentThreadId();put(r);}
void audio_call(Audio operation,uint64_t start,uint64_t end,HRESULT result) noexcept {
    if(!enabled())return;const auto elapsed=end-start;auto& total=state().audio_totals[size_t(operation)];
    ++total.calls;total.elapsed.fetch_add(elapsed);if(FAILED(result))++total.errors;
    auto maximum=total.maximum.load();while(maximum<elapsed && !total.maximum.compare_exchange_weak(maximum,elapsed)){}
    if(milliseconds(elapsed)<state().audio_threshold && SUCCEEDED(result))return;
    Record r;r.kind="audio";r.at=start;r.duration=milliseconds(elapsed);r.tid=GetCurrentThreadId();r.value=result;
    strcpy_s(r.detail,audio_names[size_t(operation)]);put(r);
}
void note(const char* message,int64_t value) noexcept {Record r;r.at=ticks();r.tid=GetCurrentThreadId();r.value=value;strncpy_s(r.detail,message,_TRUNCATE);put(r);}
void asset_call(AssetOperation operation,unsigned source,uintptr_t handle,uint64_t began,uint64_t ended,
    const char* name,unsigned name_status,uint32_t requested,uint32_t completed,bool completed_valid,uint32_t result) noexcept {
    if(!enabled() || unsigned(operation)>2 || source>1)return;
    Record r;r.kind="asset";r.id=handle;r.at=began;r.tid=GetCurrentThreadId();r.duration=milliseconds(ended-began);
    r.counts[0]=unsigned(operation);r.counts[1]=source;r.counts[2]=requested;r.counts[3]=completed;
    r.counts[4]=name_status;r.counts[5]=completed_valid;r.value=result;
    strncpy_s(r.detail,name,_TRUNCATE);put(r);
}
void reveal_event(uint64_t trace,const char* phase,uint64_t began,uint64_t ended,int32_t act,int32_t level,int32_t x,int32_t y,bool resident) noexcept {
    if(!enabled())return;Record r;r.kind="reveal";r.id=trace;r.at=began;r.tid=GetCurrentThreadId();r.duration=milliseconds(ended-began);
    r.counts[0]=uint32_t(act);r.counts[1]=uint32_t(level);r.counts[2]=uint32_t(x);r.counts[3]=uint32_t(y);r.counts[4]=resident;
    strncpy_s(r.detail,phase,_TRUNCATE);put(r);
}
void record_input(const InputResult& input) noexcept {
    if(!enabled())return;
    static std::atomic<uint64_t> sequence{0};Record r;r.kind="key_T_profile";r.id=++sequence;r.at=input.began;r.tid=GetCurrentThreadId();
    r.duration=milliseconds(input.ended-input.began);r.ms[0]=input.user_ms;r.ms[1]=input.kernel_ms;
    r.ms[2]=(input.valid&ThreadTimes)?std::max(0.0,r.duration-input.user_ms-input.kernel_ms):-1;
    r.counts[0]=input.cycles;r.counts[1]=input.read_bytes;r.counts[2]=input.read_ops;r.counts[3]=input.write_bytes;r.counts[4]=input.write_ops;
    r.counts[5]=input.other_bytes;r.counts[6]=input.other_ops;r.counts[7]=input.faults;r.counts[8]=input.valid;
    strncpy_s(r.detail,input.module,_TRUNCATE);r.value=input.module_offset;put(r);
}
void stop(bool wait) noexcept {auto& s=state();s.active=false;s.quitting=true;if(wait&&s.worker)WaitForSingleObject(s.worker,5000);}
std::wstring session_directory() {return state().directory;}
uint64_t dropped_records() noexcept {return state().dropped.load();}
}
