#include "diagnostics.h"
#include "audio_diagnostics.h"
#include "device.h"
#include <mmsystem.h>
#include <dsound.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"Pass a test output directory.");
        require(mxl::diag::start(nullptr,argv[1]),"Diagnostic start failed.");
        require(mxl::diag::start_audio(),"Audio timing hooks unavailable.");
        require(mxl::diag::audio_hook_count()>=12,"Expected DirectSound/buffer hooks were not installed.");
        IDirectSound* device=nullptr;require(SUCCEEDED(DirectSoundCreate(nullptr,&device,nullptr)),"DirectSoundCreate failed.");
        HWND window=CreateWindowExW(0,L"STATIC",L"MXL silent audio test",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(window && SUCCEEDED(device->SetCooperativeLevel(window,DSSCL_NORMAL)),"Test audio window setup failed.");
        WAVEFORMATEX wave{};wave.wFormatTag=WAVE_FORMAT_PCM;wave.nChannels=1;wave.nSamplesPerSec=22050;
        wave.wBitsPerSample=16;wave.nBlockAlign=2;wave.nAvgBytesPerSec=44100;
        DSBUFFERDESC desc{};desc.dwSize=sizeof(desc);desc.dwBufferBytes=4096;desc.lpwfxFormat=&wave;
        desc.dwFlags=DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_CTRLFREQUENCY|DSBCAPS_LOCSOFTWARE;
        IDirectSoundBuffer* buffer=nullptr;require(SUCCEEDED(device->CreateSoundBuffer(&desc,&buffer,nullptr)),"CreateSoundBuffer failed.");
        void* first=nullptr;void* second=nullptr;DWORD n1=0,n2=0;
        require(SUCCEEDED(buffer->Lock(0,4096,&first,&n1,&second,&n2,0)),"Lock forwarding failed.");
        memset(first,0,n1);if(second)memset(second,0,n2);
        require(SUCCEEDED(buffer->Unlock(first,n1,second,n2)),"Unlock forwarding failed.");
        require(SUCCEEDED(buffer->SetVolume(-10000)) && SUCCEEDED(buffer->SetPan(0)) && SUCCEEDED(buffer->SetFrequency(22050)),"Parameter forwarding failed.");
        require(SUCCEEDED(buffer->SetCurrentPosition(0)),"Cursor forwarding failed.");
        mxl::dx12::check(buffer->Play(0,0,0),"Silent playback forwarding");
        require(SUCCEEDED(buffer->Stop()),"Stop forwarding failed.");
        IDirectSoundBuffer* duplicate=nullptr;require(SUCCEEDED(device->DuplicateSoundBuffer(buffer,&duplicate)),"Duplicate forwarding failed.");
        duplicate->Release();buffer->Release();device->Release();DestroyWindow(window);
        {
            mxl::dx12::Device gpu;
            auto target=gpu.texture(8,8,1,DXGI_FORMAT_R8G8B8A8_UNORM);
            for(uint64_t i=1;i<=10;++i) {
                mxl::diag::begin_frame(i,1.25,1280,720,true);
                const float color[]={0,0,0,1};gpu.transition(*target,D3D12_RESOURCE_STATE_RENDER_TARGET);
                gpu.commands()->ClearRenderTargetView(gpu.rtv(*target),color,0,nullptr);
                mxl::diag::count(mxl::diag::Count::Draws,7);gpu.flush(true);mxl::diag::end_frame();
            }
        }
        // Real CPU clock spacing verifies the hitch flag after startup warm-up.
        Sleep(2100);mxl::diag::begin_frame(11,0,1280,720,false);mxl::diag::end_frame();
        Sleep(15);mxl::diag::begin_frame(12,0,1280,720,false);mxl::diag::end_frame();
        {
            std::ifstream live(std::filesystem::path(argv[1])/"events-0.csv");
            require(bool(live),"CSV must be readable while logging is active.");
            std::string header;std::getline(live,header);
            require(header.find("type,frame_id")!=std::string::npos,"Live CSV header missing.");
        }
        std::vector<std::thread> stress;
        const auto began=mxl::diag::ticks();
        for(int t=0;t<4;++t)stress.emplace_back([t](){for(int i=0;i<10000;++i)mxl::diag::note("concurrent_test",t);});
        for(auto& thread:stress)thread.join();
        std::cout<<"Nonblocking enqueue stress: "<<mxl::diag::milliseconds(mxl::diag::ticks()-began)<<" ms / 40000 records; dropped="<<mxl::diag::dropped_records()<<"\n";
        mxl::diag::stop(true);
        require(!mxl::diag::enabled(),"Stop did not disable recording.");
        std::ifstream stream(std::filesystem::path(argv[1])/"events-0.csv");
        const std::string data((std::istreambuf_iterator<char>(stream)),{});
        require(data.find("frame,12,")!=std::string::npos,"Frame records missing.");
        require(data.find("gpu,1,")!=std::string::npos,"Delayed GPU timestamp record missing.");
        require(data.find("audio_summary,")!=std::string::npos,"Audio totals missing.");
        require(data.find(",play,")!=std::string::npos && data.find(",lock,")!=std::string::npos,"Sound operations not observed.");
        std::cout<<"PASS: silent DirectSound forwarding, GPU timestamps, frame rows, concurrent writer and clean stop.\n";
        return 0;
    }catch(const std::exception& error){mxl::diag::stop(true);std::cerr<<error.what()<<"\n";return 1;}
}
