#include "diagnostics.h"
#include "audio_diagnostics.h"
#include "sound_probe.h"
#include "asset_probe.h"
#include "input_profile.h"
#include "reveal_probe.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool good, const char* why) { if (!good) throw std::runtime_error(why); }
}
int wmain(int argc, wchar_t** argv) {
    try {
        static_assert(!MXL_ENABLE_DIAGNOSTICS);
        require(argc==2, "Pass the isolated test directory.");
        const std::filesystem::path root=argv[1];
        wchar_t exe[32768]{}; GetModuleFileNameW(nullptr,exe,32768);
        require(std::filesystem::path(exe)==root/L"Game.exe", "Exercise the real process-name gate.");
        require(GetPrivateProfileIntW(L"Diagnostics",L"enabled",0,(root/L"mxl-diagnostics.ini").c_str())==1,
            "Fixture must request recording.");
        using namespace mxl::diag;
        require(!start(nullptr), "An old INI enabled public recording.");
        require(!start(nullptr,(root/L"forced-session").wstring()), "Test-directory bypass enabled public recording.");
        require(!enabled() && !audio_enabled() && !assets_enabled(), "Public recording flag active.");
        require(!start_audio() && !start_sound_probe() && audio_hook_count()==0, "Audio instrumentation activated.");
        const auto began=ticks();
        require(began!=0 && milliseconds(began)>0, "Shared clock helpers stopped working.");
        for (unsigned i=0;i<256;++i) {
            Scope scope(Metric::Render);
            begin_frame(i,2,1024,768,false); count(Count::Draws); add(Metric::Upload,1);
            producer(i,began,began+1,1,4); gpu_batch(i,1);
            audio_call(Audio::Play,began,began+1,S_OK);
            native_sound_call(NativeSound::AsyncLoad,began,began+1,0,0,0,0,"sample.wav",0);
            asset_call(AssetOperation::Open,0,1,began,began+1,"item.dc6",0,0,0,false,1);
            reveal_event(i,"test",began,began+1,1,1,0,0,true);
            record_input(InputResult{}); note("public test",i); end_frame();
        }
        stop(true);
        require(session_directory().empty() && current_frame()==0 && dropped_records()==0, "Public recorder state exists.");
        unsigned files=0;
        for (const auto& file:std::filesystem::directory_iterator(root)) {
            require(file.is_regular_file(), "Public build created an output directory.");
            const auto name=file.path().filename();
            require(name==L"Game.exe" || name==L"mxl-diagnostics.ini", "Public build wrote an output file.");
            ++files;
        }
        require(files==2,"Fixture files changed.");
        std::cout << "Public recording remains off with enabled INI and forced session; no audio hooks or output files.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
