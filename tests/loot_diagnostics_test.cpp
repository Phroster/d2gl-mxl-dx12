// SPDX-License-Identifier: GPL-3.0-or-later
#include "diagnostics.h"
#include "audio_diagnostics.h"
#include "asset_probe.h"
#include "device.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

static void require(bool value,const char* reason) { if(!value)throw std::runtime_error(reason); }
static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> values;std::stringstream stream(line);std::string value;
    while(std::getline(stream,value,','))values.push_back(value);
    return values;
}
int wmain(int argc,wchar_t** argv) {
    using namespace mxl::diag;
    try {
        require(argc==2,"Pass the isolated automatic-start fixture root.");
        // No forced test directory: exercise the installed Game.exe + INI path.
        require(start(nullptr),"Automatic Game.exe recording did not start.");
        require(!audio_enabled() && !assets_enabled() && !detail_logs_enabled(),"Unrequested probes enabled.");
        require(comprehensive_enabled(),"Comprehensive configuration not honored.");
        require(!start_audio() && audio_hook_count()==0,"Frame-only setup attached audio probes.");
        const auto directory=std::filesystem::path(session_directory());
        require(directory.parent_path()==std::filesystem::path(argv[1])/L"mxl-diagnostics","Unexpected session destination.");
        require(std::filesystem::exists(directory/L"mxl-native-loot.ini"),"Launch settings not snapshotted.");
        auto submit=[](uint64_t id) {producer_ready();const auto now=ticks();producer(id,now,now+1,6.94,42,1.25);};
        producer_set(Count::LootEnabled,0);submit(1);
        producer_begin();Sleep(1);producer_stage(2);Sleep(1);producer_stage(1);
        producer_set(Count::LootEnabled,1);producer_set(Count::LootTargets,60);
        producer_count(Count::LootLabels,60);producer_count(Count::LootCacheUploads,124);
        producer_set(Count::MotionValid,1);producer_set(Count::MotionSamples,27);
        producer_set(Count::MotionElapsedTicks,600001);producer_set(Count::MotionClampedTicks,600000);
        producer_set(Count::MotionIntervalTicks,400000);producer_set(Count::MotionPlayerValid,1);
        producer_set(Count::MotionPlayerX,123456789);producer_set(Count::MotionCameraX,uint32_t(-200));
        producer_set(Count::MotionEpochRawTicks,123456788999ULL);producer_set(Count::MotionUpdateTicks,123456780000ULL);
        producer_set(Count::MotionEpochSamples,45);producer_set(Count::MotionEpochResets,2);
        producer_set(Count::MotionEpochReason,2);producer_set(Count::MotionEpochActive,1);
        producer_set(Count::MotionRenderRawTicks,123456789123ULL);producer_set(Count::MotionRenderNowTicks,123456789456ULL);
        producer_set(Count::MotionRenderResets,3);producer_set(Count::MotionRenderActive,1);
        producer_set(Count::ContextValid,1);producer_set(Count::Level,40);producer_set(Count::PlayerMode,6);
        for(unsigned i=0;i<128;++i) {
            ProducerSampleScope sampled(Metric::LootPickupSampled,Count::LootSelectionCalls,Count::LootSelectionSamples);
        }
        {ProducerScope labels(Metric::LootLabels);Sleep(1);}
        // Render-thread metrics and another producer must not contaminate the
        // game thread's totals. Submissions have independently reset samples.
        std::thread worker([&] {producer_count(Count::LootTargets,7);submit(20);});worker.join();
        begin_frame(2,0,1280,720,false);count(Count::Draws,3);end_frame();
        submit(2);
        input_event("primary_down",ticks());
        producer_set(Count::LootEnabled,1);submit(3);
        // A hidden test window exercises the real DXGI Present forwarding.
        // Occluded/unsupported statistics remain explicit, not fabricated.
        HWND window=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"Private recorder test",WS_POPUP,
            0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(window!=nullptr,"Hidden presentation window failed.");
        {
            mxl::dx12::Device gpu(window);
            begin_frame(30,0,64,64,false);
            gpu.present(false);end_frame();
        }
        DestroyWindow(window);
        stop(true);require(!enabled(),"Recorder failed to stop.");
        std::ifstream file(directory/L"events-0.csv");std::string line;
        std::getline(file,line);const auto columns=split(line);
        std::map<std::string,size_t> index;
        for(size_t i=0;i<columns.size();++i)index[columns[i]]=i;
        std::map<int,std::vector<std::string>> rows;bool process_seen=false,input_seen=false,present_seen=false;
        while(std::getline(file,line)) {
            auto values=split(line);
            if(values.size()==columns.size() && values[0]=="producer")rows[std::stoi(values[1])]=values;
            if(values.size()==columns.size() && values[0]=="process")process_seen=std::stoull(values[index.at("process_valid")])!=0;
            if(values.size()==columns.size() && values[0]=="input_event")input_seen=values[index.at("detail")]=="primary_down";
            if(values.size()==columns.size() && values[0]=="frame" && values[1]=="30") {
                present_seen=std::stoull(values[index.at("present_start")])>0
                    && std::stoull(values[index.at("present_end")])>=std::stoull(values[index.at("present_start")]);
                std::cout<<"DXGI presentation smoke: result="<<values[index.at("present_result")]
                    <<" stats_queried="<<values[index.at("present_probed")]
                    <<" stats_result="<<values[index.at("present_stats_result")]<<'\n';
            }
        }
        auto number=[&](int id,const char* key) {return std::stod(rows.at(id).at(index.at(key)));};
        require(number(1,"loot_enabled")==0 && number(1,"loot_targets")==0,"Disabled baseline mislabeled.");
        require(number(2,"loot_enabled")==1 && number(2,"loot_targets")==60 && number(2,"loot_labels")==60,"Visible totals missing.");
        require(number(2,"loot_selection_calls")==128 && number(2,"loot_selection_samples")==2,"Sampling did not count all calls and time one in 64.");
        require(number(2,"loot_labels_ms")>0 && number(2,"loot_cache_uploads")==124,"Timing/cache totals missing.");
        require(number(2,"motion_valid")==1 && number(2,"motion_samples")==27 && number(2,"motion_elapsed_ticks")==600001
            && number(2,"motion_clamped_ticks")==600000 && number(2,"motion_interval_ticks")==400000
            && number(2,"motion_player_x")==123456789 && number(2,"motion_camera_x")==uint32_t(-200),"Motion values changed during logging.");
        require(number(3,"loot_enabled")==1 && number(3,"loot_targets")==0 && number(3,"loot_selection_calls")==0 && number(3,"loot_labels_ms")==0,"Counters leaked across producer frames.");
        require(number(20,"loot_targets")==7 && number(20,"loot_labels")==0,"Totals crossed thread boundaries.");
        require(number(3,"motion_valid")==0 && number(3,"motion_player_valid")==0,"Missing motion sample was treated as valid.");
        require(number(2,"motion_render_raw_ticks")==123456789123ULL && number(2,"motion_render_now_ticks")==123456789456ULL
            && number(2,"motion_render_resets")==3 && number(2,"motion_render_active")==1,"Render clock columns lost.");
        require(number(2,"motion_epoch_raw_ticks")==123456788999ULL && number(2,"motion_update_ticks")==123456780000ULL
            && number(2,"motion_epoch_samples")==45 && number(2,"motion_epoch_resets")==2
            && number(2,"motion_epoch_reason")==2 && number(2,"motion_epoch_active")==1
            && number(3,"motion_epoch_active")==0,"Visual clock observation changed or leaked into a missing sample.");
        require(number(2,"game_world_ms")>0 && number(2,"game_map_ms")>0 && number(2,"game_ui_ms")>0,"Draw stages not measured.");
        require(number(2,"context_valid")==1 && number(2,"level")==40 && number(2,"player_mode")==6,"Area/action context missing.");
        require(number(2,"build_cycles_valid")==1 && number(2,"probe_ms")>0,"CPU cycles/probe timing missing.");
        require(number(3,"build_cycles_valid")==0 && number(3,"game_world_ms")==0,"CPU/stage values leaked across frames.");
        require(process_seen && input_seen,"Background health or action records missing.");
        require(present_seen,"Real presentation record missing.");
        std::ifstream metadata(directory/L"session.txt");const std::string meta((std::istreambuf_iterator<char>(metadata)),{});
        require(meta.find("event_parts=16")!=std::string::npos && meta.find("event_capacity_mb=512")!=std::string::npos,"Long-run retention missing.");
        require(dropped_records()==0,"Automatic recording fixture dropped records.");
        std::cout << "PASS: automatic Game.exe recording, frame-only configuration, launch snapshot, empty/visible loot, sampled calls, thread isolation and flush.\n";
    }catch(const std::exception& error){stop(true);std::cerr<<error.what()<<'\n';return 1;}
}
