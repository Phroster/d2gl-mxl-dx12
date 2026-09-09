// SPDX-License-Identifier: GPL-3.0-or-later
#include "diagnostics.h"
#include "audio_diagnostics.h"
#include "asset_probe.h"
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
        require(!start_audio() && audio_hook_count()==0,"Frame-only setup attached audio probes.");
        const auto directory=std::filesystem::path(session_directory());
        require(directory.parent_path()==std::filesystem::path(argv[1])/L"mxl-diagnostics","Unexpected session destination.");
        require(std::filesystem::exists(directory/L"mxl-native-loot.ini"),"Launch settings not snapshotted.");
        auto submit=[](uint64_t id) {const auto now=ticks();producer(id,now,now+1,6.94,42,1.25);};
        producer_set(Count::LootEnabled,0);submit(1);
        producer_set(Count::LootEnabled,1);producer_set(Count::LootTargets,60);
        producer_count(Count::LootLabels,60);producer_count(Count::LootCacheUploads,124);
        for(unsigned i=0;i<128;++i) {
            ProducerSampleScope sampled(Metric::LootPickupSampled,Count::LootSelectionCalls,Count::LootSelectionSamples);
        }
        {ProducerScope labels(Metric::LootLabels);Sleep(1);}
        // Render-thread metrics and another producer must not contaminate the
        // game thread's totals. Submissions have independently reset samples.
        std::thread worker([&] {producer_count(Count::LootTargets,7);submit(20);});worker.join();
        begin_frame(2,0,1280,720,false);count(Count::Draws,3);end_frame();
        submit(2);
        producer_set(Count::LootEnabled,1);submit(3);
        stop(true);require(!enabled(),"Recorder failed to stop.");
        std::ifstream file(directory/L"events-0.csv");std::string line;
        std::getline(file,line);const auto columns=split(line);
        std::map<std::string,size_t> index;
        for(size_t i=0;i<columns.size();++i)index[columns[i]]=i;
        std::map<int,std::vector<std::string>> rows;
        while(std::getline(file,line)) {
            auto values=split(line);
            if(values.size()==columns.size() && values[0]=="producer")rows[std::stoi(values[1])]=values;
        }
        auto number=[&](int id,const char* key) {return std::stod(rows.at(id).at(index.at(key)));};
        require(number(1,"loot_enabled")==0 && number(1,"loot_targets")==0,"Disabled baseline mislabeled.");
        require(number(2,"loot_enabled")==1 && number(2,"loot_targets")==60 && number(2,"loot_labels")==60,"Visible totals missing.");
        require(number(2,"loot_selection_calls")==128 && number(2,"loot_selection_samples")==2,"Sampling did not count all calls and time one in 64.");
        require(number(2,"loot_labels_ms")>0 && number(2,"loot_cache_uploads")==124,"Timing/cache totals missing.");
        require(number(3,"loot_enabled")==1 && number(3,"loot_targets")==0 && number(3,"loot_selection_calls")==0 && number(3,"loot_labels_ms")==0,"Counters leaked across producer frames.");
        require(number(20,"loot_targets")==7 && number(20,"loot_labels")==0,"Totals crossed thread boundaries.");
        require(dropped_records()==0,"Automatic recording fixture dropped records.");
        std::cout << "PASS: automatic Game.exe recording, frame-only configuration, launch snapshot, empty/visible loot, sampled calls, thread isolation and flush.\n";
    }catch(const std::exception& error){stop(true);std::cerr<<error.what()<<'\n';return 1;}
}
