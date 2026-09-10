#include "diagnostics.h"
#include "input_profile.h"
#include "reveal_probe.h"
#include "asset_probe.h"
#include "sound_probe.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <share.h>
#include <psapi.h>

namespace mxl::diag {
namespace {
constexpr size_t Metrics=size_t(Metric::Count), Counts=size_t(Count::Count), AudioOps=size_t(Audio::Count), NativeOps=size_t(NativeSound::Count), Capacity=8192;
const char* metric_names[]={"render_ms","input_wait_ms","gpu_fence_wait_ms","present_call_ms","latency_wait_ms","submit_ms","pipeline_ms","bindings_ms","index_scan_ms","upload_ms","allocation_ms","producer_build_ms",
    "loot_effects_ms","loot_labels_ms","loot_names_ms","loot_pickup_sampled_ms","loot_capture_sampled_ms",
    "game_world_ms","game_ui_ms","game_map_ms","probe_ms","present_probe_ms"};
const char* count_names[]={"draws","indices","texture_bytes","buffer_bytes","spill_bytes","new_pipelines","binding_misses","barriers","minimap","width","height","game_screen","new_textures",
    "loot_enabled","loot_pickup_enabled","loot_targets","loot_labels","loot_sprites","loot_name_formats",
    "loot_selection_calls","loot_selection_samples","loot_capture_calls","loot_capture_samples",
    "loot_inventory_queries","loot_unit_lookups","loot_cache_reclaims","loot_cache_uploads","loot_cache_hits","loot_cache_skipped",
    "motion_valid","motion_samples","motion_elapsed_ticks","motion_interval_ticks","motion_clamped_ticks",
    "motion_client_updates","motion_update_ms","motion_clock_ms","motion_game_type",
    "motion_player_valid","motion_player_id","motion_player_x","motion_player_y","motion_camera_x","motion_camera_y","motion_panels",
    "context_valid","level","player_mode","world_units","world_players","world_monsters","world_missiles","world_items",
    "motion_render_ticks","motion_update_ticks","motion_probe_ticks",
    "build_cycles_valid","build_cycles","wait_cycles_valid","wait_cycles","render_cycles_valid","render_cycles",
    "present_start","present_end","present_result","present_vsync","present_probed","present_stats_result",
    "present_stats_count","present_refresh","sync_refresh","sync_qpc","present_id_valid","present_id","latency_result",
    "process_valid","process_user","process_kernel","process_read","process_write","process_faults","working_set","private_bytes",
    "logger_cpu_valid","logger_cpu","queue_peak","queue_size",
    "motion_epoch_raw_ticks","motion_epoch_samples","motion_epoch_resets","motion_epoch_reason","motion_epoch_active",
    "motion_render_raw_ticks","motion_render_now_ticks","motion_render_resets","motion_render_active"};
const char* audio_names[]={"factory","create_buffer","duplicate_buffer","play","stop","lock","unlock","volume","pan","frequency","cursor","restore","parameters_3d","position_3d","commit_3d","get_status","get_current_position","release","query_interface"};
const char* native_names[]={"async_load","async_buffer","async_free","client_open","client_read","client_close","sound_lock_wait","sound_lock_hold","sound_wait","sound_sleep","music_begin","music_end","music_position","client_wait","client_sleep","async_ready"};
static_assert(std::size(native_names)==NativeOps);
static_assert(std::size(metric_names)==Metrics && std::size(count_names)==Counts && std::size(audio_names)==AudioOps);
struct Record {
    const char* kind="note"; uint64_t id=0,at=0; uint32_t tid=0;
    double duration=0,interval=0; bool focused=false,slow=false;
    std::array<double,Metrics> ms{}; std::array<uint64_t,Counts> counts{};
    char detail[96]{}; int64_t value=0;
};
struct AudioTotals { std::atomic<uint64_t> calls{0},elapsed{0},maximum{0},errors{0}; };
struct State {
    std::atomic<bool> active{false},quitting{false}; bool audio=true,assets=false,details=false,testing=false,comprehensive=false;
    uint32_t event_parts=3;
    std::atomic<uint64_t> dropped{0}; HWND window=nullptr; HANDLE worker=nullptr,owner=nullptr;
    LARGE_INTEGER frequency{}; uint64_t started=0; double threshold=10.0,audio_threshold=.5;
    std::wstring directory; SRWLOCK lock=SRWLOCK_INIT;
    std::array<Record,Capacity> queue{}; size_t read=0,write=0,size=0,peak=0;
    std::array<AudioTotals,AudioOps> audio_totals{};
    std::array<AudioTotals,NativeOps> native_totals{};
};
State& state() { static State* s=new State;return *s; }
thread_local Record frame;
thread_local Record producer_work;
thread_local bool frame_active=false;
thread_local uint64_t previous_end=0;
thread_local bool previous_focused=false;
thread_local uint32_t previous_screen=0;
struct CycleSample {uint64_t value=0;bool valid=false;};
thread_local CycleSample build_cycles,wait_cycles,render_cycles;
thread_local uint64_t stage_start=0;
thread_local unsigned draw_stage=0;
CycleSample cycle_sample() noexcept {
    const auto error=GetLastError();CycleSample sample;
    sample.valid=QueryThreadCycleTime(GetCurrentThread(),&sample.value)!=FALSE;
    SetLastError(error);return sample;
}
void cycle_delta(Record& record,CycleSample before,CycleSample after,Count valid,Count value) {
    if(before.valid && after.valid && after.value>=before.value) {
        record.counts[size_t(valid)]=1;record.counts[size_t(value)]=after.value-before.value;
    }
}
void put(const Record& record) noexcept {
    auto& s=state();
    if(!s.active.load(std::memory_order_relaxed)) return;
    if(!TryAcquireSRWLockExclusive(&s.lock)){++s.dropped;return;}
    if(s.size==Capacity) ++s.dropped;
    else {s.queue[s.write]=record;s.write=(s.write+1)%Capacity;++s.size;s.peak=std::max(s.peak,s.size);}
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
    FILE* file=nullptr;FILE* inputs=nullptr;FILE* reveals=nullptr;FILE* assets=nullptr;FILE* native=nullptr;
    uint32_t part=0,input_count=0,reveal_count=0,asset_count=0,native_count=0;uint64_t last_summary=ticks(),written=0,slow=0;
    auto open=[&](){
        const auto name=s.directory+L"\\events-"+std::to_wstring(part%s.event_parts)+L".csv";
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
            if(!strcmp(batch[i].kind,"native_sound")) {
                if(!native && native_count==0){
                    native=_wfsopen((s.directory+L"\\native-sound.csv").c_str(),L"wb",_SH_DENYNO);
                    if(native)fputs("operation,session_ms,thread_id,duration_ms,caller,object,argument,result,path,path_status\n",native);
                }
                if(native && native_count<131072) {
                    const auto& r=batch[i];
                    fprintf(native,"%s,%.6f,%u,%.6f,%llu,%llu,%llu,%lld,\"",native_names[r.counts[0]],milliseconds(r.at-s.started),r.tid,r.duration,
                        (unsigned long long)r.counts[1],(unsigned long long)r.counts[2],(unsigned long long)r.counts[3],(long long)r.value);
                    for(const char* c=r.detail;*c;++c){if(*c=='\"')fputc('\"',native);fputc(*c,native);}
                    fprintf(native,"\",%llu\n",(unsigned long long)r.counts[4]);++native_count;
                }else ++s.dropped;
            } else if(!strcmp(batch[i].kind,"asset")) {
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
            for(size_t i=0;i<NativeOps;++i) {
                auto& a=s.native_totals[i];const auto calls=a.calls.exchange(0),elapsed=a.elapsed.exchange(0),maximum=a.maximum.exchange(0);
                if(calls || elapsed || maximum) {
                    Record r;r.kind="native_sound_summary";r.at=now;r.duration=milliseconds(elapsed);r.interval=milliseconds(maximum);
                    r.counts[0]=calls;strcpy_s(r.detail,native_names[i]);write_record(file,r);
                }
            }
            if(s.comprehensive) {
                const auto began=ticks();Record process;process.kind="process";process.at=now;process.tid=GetCurrentThreadId();
                auto set=[&](Count key,uint64_t value){process.counts[size_t(key)]=value;};
                auto ft=[](FILETIME value){return (uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime;};
                FILETIME created{},exited{},kernel{},user{};uint64_t valid=0;
                if(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)) {
                    valid|=1;set(Count::ProcessUser,ft(user));set(Count::ProcessKernel,ft(kernel));
                }
                IO_COUNTERS io{};
                if(GetProcessIoCounters(GetCurrentProcess(),&io)) {valid|=2;set(Count::ProcessRead,io.ReadTransferCount);set(Count::ProcessWrite,io.WriteTransferCount);}
                PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
                if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))) {
                    valid|=4;set(Count::ProcessFaults,memory.PageFaultCount);set(Count::WorkingSet,memory.WorkingSetSize);set(Count::PrivateBytes,memory.PrivateUsage);
                }
                set(Count::ProcessValid,valid);
                if(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)) {set(Count::LoggerCpuValid,1);set(Count::LoggerCpu,ft(user)+ft(kernel));}
                process.ms[size_t(Metric::Probe)]=milliseconds(ticks()-began);write_record(file,process);
            }
            Record status;status.kind="logger";status.at=now;status.value=s.dropped.load();
            AcquireSRWLockExclusive(&s.lock);status.counts[size_t(Count::QueuePeak)]=s.peak;status.counts[size_t(Count::QueueSize)]=s.size;ReleaseSRWLockExclusive(&s.lock);
            strcpy_s(status.detail,"dropped_records");write_record(file,status);
            fflush(file);if(inputs)fflush(inputs);if(reveals)fflush(reveals);if(assets)fflush(assets);if(native)fflush(native);last_summary=now;
            FILE* out=nullptr;const auto path=s.directory+L"\\status.txt";
            if(!_wfopen_s(&out,path.c_str(),L"wb") && out) {
                fprintf(out,"MXL Smooth Motion DX12 1.15 diagnostics\nstate=%s\nrecords=%llu\nslow_frames=%llu\ndropped_records=%llu\nevent_parts_written=%u\nretention_overwrites=%u\n",
                    s.quitting?"stopped":"recording",(unsigned long long)written,(unsigned long long)slow,(unsigned long long)s.dropped.load(),part+1,part>=s.event_parts?part-s.event_parts+1:0);fclose(out);
            }
            if(GetFileAttributesW((s.directory+L"\\STOP").c_str())!=INVALID_FILE_ATTRIBUTES) {
                s.active=false;s.quitting=true;
            }
        }
        if(s.quitting && count==0) break;
        if(count==0) Sleep(50);
    }
    if(inputs){fflush(inputs);fclose(inputs);}if(reveals){fflush(reveals);fclose(reveals);}
    if(native){fflush(native);fclose(native);}
    if(assets){fflush(assets);fclose(assets);}fflush(file);fclose(file);return 0;
}
}
uint64_t ticks() noexcept {LARGE_INTEGER t;QueryPerformanceCounter(&t);return uint64_t(t.QuadPart);}
double milliseconds(uint64_t elapsed) noexcept {return state().frequency.QuadPart?double(elapsed)*1000.0/state().frequency.QuadPart:0;}
bool enabled() noexcept {return state().active.load(std::memory_order_relaxed);}
bool audio_enabled() noexcept {return enabled()&&state().audio;}
bool detail_logs_enabled() noexcept {return enabled()&&state().details;}
bool comprehensive_enabled() noexcept {return enabled()&&state().comprehensive;}
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
    s.details=GetPrivateProfileIntW(L"Diagnostics",L"detail_logs",0,ini.c_str())!=0;
    s.comprehensive=GetPrivateProfileIntW(L"Diagnostics",L"comprehensive",0,ini.c_str())!=0;
    s.event_parts=std::clamp(GetPrivateProfileIntW(L"Diagnostics",L"event_parts",3,ini.c_str()),3u,32u);
    s.threshold=std::clamp(GetPrivateProfileIntW(L"Diagnostics",L"slow_frame_ms",10,ini.c_str()),5u,1000u);
    QueryPerformanceFrequency(&s.frequency);s.started=ticks();s.window=window;s.testing=!test_directory.empty();
    SYSTEMTIME utc{},local{};GetSystemTime(&utc);GetLocalTime(&local);wchar_t session[80];
    swprintf_s(session,L"%04u%02u%02u-%02u%02u%02u-pid%lu",local.wYear,local.wMonth,local.wDay,local.wHour,local.wMinute,local.wSecond,GetCurrentProcessId());
    s.directory=test_directory.empty()?(root/L"mxl-diagnostics"/session).wstring():test_directory;
    std::error_code error;std::filesystem::create_directories(s.directory,error);if(error)return false;
    // Snapshot only small, named settings files once at launch. This records
    // the native filter's saved configuration without polling disk in play.
    auto snapshot=[&](const std::filesystem::path& source,const wchar_t* name) {
        std::error_code ec;
        const auto size=std::filesystem::file_size(source,ec);
        if(!ec && size<=8*1024*1024)
            std::filesystem::copy_file(source,std::filesystem::path(s.directory)/name,
                std::filesystem::copy_options::overwrite_existing,ec);
    };
    for(const auto* name:{L"mxl-native-loot.ini",L"d2gl.ini",L"d2fps.ini",L"mxl-diagnostics.ini"})snapshot(root/name,name);
    if(test_directory.empty()) {
        wchar_t appdata[32768]{};
        const auto length=GetEnvironmentVariableW(L"APPDATA",appdata,DWORD(std::size(appdata)));
        if(length && length<std::size(appdata))
            snapshot(std::filesystem::path(appdata)/L"MedianXL"/L"save"/L"lootfilterconf.json",L"lootfilterconf.json");
    }
    FILE* file=nullptr;
    if(!_wfopen_s(&file,(s.directory+L"\\session.txt").c_str(),L"wb") && file) {
        fprintf(file,"MXL Smooth Motion DX12 1.15 diagnostics\npid=%lu\nqpc_frequency=%lld\nqpc_start=%llu\n",
            GetCurrentProcessId(),(long long)s.frequency.QuadPart,(unsigned long long)s.started);
        fputs("build_kind=MXL_PRIVATE_LOOT_DIAGNOSTICS_V1\nSaved filter/settings snapshots are launch-time state only; later menu changes are not observed directly.\n",file);
        fprintf(file,"comprehensive_schema=1\ncomprehensive=%u\nevent_parts=%u\nevent_capacity_mb=%u\n",unsigned(s.comprehensive),s.event_parts,s.event_parts*32);
        fputs("Draw stages cover producer drawing only; world/UI/map timings include nested loot costs. World-unit counts are hook visits, not unique entities. Player mode is the native action mode; level is the native area ID.\n"
              "Build/wait/render cycle values are CPU cycles with separate validity flags, not milliseconds. Probe times include new clock/counter reads; logger CPU is sampled on its background writer.\n"
              "Process CPU and I/O are cumulative and include the recorder; CPU FILETIME units are 100 ns. Faults include soft faults. I/O counters are not physical disk traffic. Process-valid bits: 1 CPU, 2 I/O, 4 memory.\n"
              "Input events record categories and handler wall time only, without key text. They do not measure input-to-display latency.\n"
              "Presentation columns identify Present call start/end/result and submitted present ID. DXGI frame statistics may describe an older present, be unsupported, or be disjoint. SyncQPC is a refresh synchronization point, not a timestamp for this exact rendered frame. HRESULT/valid fields must be checked.\n"
              "motion_render_ticks and motion_update_ticks are the verified D2FPS presentation timeline and game epoch. motion_probe_ticks is QPC at the snapshot.\n"
              "epoch_schema=1\n"
              "render_clock_schema=1\n"
              "motion_render_raw_ticks is the original limiter timestamp; motion_render_now_ticks is its exact QPC input. motion_render_ticks is the visual timestamp after correction.\n"
              "motion_epoch_raw_ticks is the original clock conversion before our visual epoch correction. motion_update_ticks is the epoch actually stored in D2FPS.\n"
              "motion_epoch_samples/resets are cumulative update-hook counts. reason: 0 passthrough, 1 first anchor, 2 continuous step, 3 area, 4 discontinuity/pause, 5 over 20 ms disagreement. motion_epoch_active marks realm correction.\n",file);
        fputs("motion_schema=2\nMotion fields observe the signed visual phase. motion_valid=0 means unavailable.\n"
              "motion_samples is a cumulative clamp-call counter; unchanged means the recorded clamp values are stale for that producer row.\n"
              "Elapsed/clamped ticks are signed int64 values encoded as uint64; interval ticks are positive. Convert using qpc_frequency. Both elapsed signs enter this clamp.\n"
              "Client updates/time are read from the verified D2Client loop once per draw; clock_ms uses the matching timeGetTime clock.\n"
              "Player coordinates are unsigned 16.16 path coordinates when the local player reaches the existing world-draw hook; motion_player_valid=0 means not observed.\n"
              "Camera coordinates are signed 32-bit pixels encoded as uint32. Compare movement only across adjacent frame IDs with unchanged player ID and panels. Stationary frames alone do not prove a motion stall.\n",file);
        fprintf(file,"utc_start=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\nlocal_start=%04u-%02u-%02u %02u:%02u:%02u.%03u\n",
            utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds,
            local.wYear,local.wMonth,local.wDay,local.wHour,local.wMinute,local.wSecond,local.wMilliseconds);
        fprintf(file,"slow_frame_ms=%.2f\naudio=%u\nassets=%u\ndetail_logs=%u\nGPU timestamps cover our DX12 command lists, not ReShade's separate submissions.\n",
            s.threshold,unsigned(s.audio),unsigned(s.assets),unsigned(s.details));
        fputs("Producer loot columns cover work since the previous producer submission, on that thread only. Match frame_id with frame/GPU rows.\n"
              "loot_enabled=0 is effects disabled; loot_enabled=1 with loot_targets=0 is enabled but no effects drawn. Native filter activation is a separate game setting.\n"
              "loot_effects_ms covers native effect submission and lookups; loot_labels_ms includes names/layout/font drawing; loot_names_ms is nested formatting only. Do not sum nested timings.\n"
              "Pickup and capture timing samples the first and then every 64th call per producer interval. *_calls counts every call; *_samples counts timed calls; *_sampled_ms is sampled wall time only, not total cost.\n"
              "Loot cache counts are per-frame deltas; uploads/hits refer to immutable loot cells, reclaims/skips include ordinary sprites.\n",file);
        fputs("Audio summaries: duration_ms=sum of completed call wall times; interval_ms=largest call; draws=calls; indices=failed calls.\n"
              "native-sound.csv: verified Client/Fog async load/ready/get/free and file I/O; D2Sound lock acquire/outer hold, waits, sleeps and Storm music.\n"
              "Native details: calls at least 0.5 ms plus every async load/free and client open/close; at most 131072 rows. Paths bounded to 95 bytes.\n"
              "Native path_status: 0=complete, 1=truncated, 2=unreadable, 3=not a path operation. Object IDs may be reused; async load/free bound job lifetimes.\n"
              "Native caller is an absolute return address; native_sound_client_base/module_base notes allow module-relative attribution.\n"
              "Native argument: async load priority (signed 32-bit), file read byte count, wait timeout or sleep duration; result is unchanged native return bits, zero for void calls.\n"
              "Native summaries: duration_ms=sum, interval_ms=max, draws=calls. Scopes nest; do not add lock holds to contained calls. Worker waits alone are not game-thread stalls.\n"
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
              "Event CSV retention uses event_parts files of 32 MiB; overwrites are reported in status.txt. Create an empty STOP file here to stop recording.\n",file);fclose(file);
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
    if(comprehensive_enabled()){const auto began=ticks();render_cycles=cycle_sample();frame.ms[size_t(Metric::Probe)]+=milliseconds(ticks()-began);}
}
void end_frame() noexcept {
    if(!frame_active)return;const auto end=ticks();frame.duration=milliseconds(end-frame.at);
    frame.interval=previous_end?milliseconds(end-previous_end):0;previous_end=end;
    frame.focused=state().testing || GetForegroundWindow()==state().window;
    const auto screen=uint32_t(frame.counts[size_t(Count::GameScreen)]);
    frame.slow=frame.focused && previous_focused && screen==1 && previous_screen==1 && frame.interval>=state().threshold && milliseconds(end-state().started)>2000;
    previous_focused=frame.focused;previous_screen=screen;
    if(comprehensive_enabled()){const auto began=ticks();cycle_delta(frame,render_cycles,cycle_sample(),Count::RenderCyclesValid,Count::RenderCycles);frame.ms[size_t(Metric::Probe)]+=milliseconds(ticks()-began);}
    put(frame);frame_active=false;
}
void add(Metric metric,uint64_t elapsed) noexcept {if(frame_active)frame.ms[size_t(metric)]+=milliseconds(elapsed);}
void count(Count metric,uint64_t amount) noexcept {if(frame_active)frame.counts[size_t(metric)]+=amount;}
void producer(uint64_t id,uint64_t ready,uint64_t returned,double interval,uint32_t vertices,double build_ms) noexcept {
    if(comprehensive_enabled()){const auto began=ticks();cycle_delta(producer_work,wait_cycles,cycle_sample(),Count::WaitCyclesValid,Count::WaitCycles);producer_add(Metric::Probe,ticks()-began);wait_cycles={};}
    if(!enabled())return;Record r=producer_work;producer_work={};r.kind="producer";r.id=id;r.at=ready;r.tid=GetCurrentThreadId();
    r.duration=milliseconds(returned-ready);r.interval=interval;r.counts[0]=vertices;r.ms[size_t(Metric::ProducerBuild)]=build_ms;put(r);
}
void producer_begin() noexcept {
    if(!comprehensive_enabled())return;
    const auto began=ticks();build_cycles=cycle_sample();wait_cycles={};draw_stage=0;stage_start=ticks();
    producer_add(Metric::Probe,stage_start-began);
}
void producer_stage(unsigned stage) noexcept {
    if(!comprehensive_enabled())return;
    const auto now=ticks();const Metric metrics[]={Metric::GameWorld,Metric::GameUI,Metric::GameMap};
    if(stage_start)producer_add(metrics[draw_stage],now-stage_start);
    draw_stage=std::min(stage,2u);stage_start=now;
}
void producer_ready() noexcept {
    if(!comprehensive_enabled())return;
    producer_stage(draw_stage);stage_start=0;
    const auto began=ticks();const auto sample=cycle_sample();
    cycle_delta(producer_work,build_cycles,sample,Count::BuildCyclesValid,Count::BuildCycles);build_cycles={};wait_cycles=sample;
    producer_add(Metric::Probe,ticks()-began);
}
void input_event(const char* category,uint64_t began) noexcept {
    if(!comprehensive_enabled() || !began)return;
    Record r;r.kind="input_event";r.at=began;r.tid=GetCurrentThreadId();r.duration=milliseconds(ticks()-began);
    strncpy_s(r.detail,category,_TRUNCATE);put(r);
}
void producer_add(Metric metric,uint64_t elapsed) noexcept {if(enabled())producer_work.ms[size_t(metric)]+=milliseconds(elapsed);}
void producer_count(Count metric,uint64_t amount) noexcept {if(enabled())producer_work.counts[size_t(metric)]+=amount;}
void producer_set(Count metric,uint64_t amount) noexcept {if(enabled())producer_work.counts[size_t(metric)]=amount;}
bool producer_sample(Count calls,Count samples) noexcept {
    if(!enabled())return false;
    const auto count=++producer_work.counts[size_t(calls)];
    if((count-1)%64)return false;
    ++producer_work.counts[size_t(samples)];return true;
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
void native_sound_call(NativeSound operation,uint64_t began,uint64_t ended,uintptr_t caller,uintptr_t object,
    uint32_t argument,uintptr_t result,const char* path,unsigned path_status) noexcept {
    if(!audio_enabled() || size_t(operation)>=NativeOps)return;
    const auto elapsed=ended-began;auto& total=state().native_totals[size_t(operation)];
    ++total.calls;total.elapsed.fetch_add(elapsed);
    auto maximum=total.maximum.load();while(maximum<elapsed && !total.maximum.compare_exchange_weak(maximum,elapsed)){}
    const bool lifetime=operation==NativeSound::AsyncLoad || operation==NativeSound::AsyncFree ||
        operation==NativeSound::ClientOpen || operation==NativeSound::ClientClose;
    if(!lifetime && milliseconds(elapsed)<state().audio_threshold)return;
    Record r;r.kind="native_sound";r.at=began;r.tid=GetCurrentThreadId();r.duration=milliseconds(elapsed);
    r.counts[0]=unsigned(operation);r.counts[1]=caller;r.counts[2]=object;r.counts[3]=argument;r.counts[4]=path_status;r.value=result;
    strncpy_s(r.detail,path,_TRUNCATE);for(char& c:r.detail)if(c=='\r' || c=='\n')c=' ';
    put(r);
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
