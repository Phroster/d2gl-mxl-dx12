#include "backend_state.h"
#include "diagnostics.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace mxl::dx12;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void verify_upload(const Upload& up,const std::vector<uint8_t>& expected) {
    auto readback=gpu().buffer(expected.size(),D3D12_HEAP_TYPE_READBACK);
    gpu().commands()->CopyBufferRegion(readback->object.Get(),0,up.resource,up.offset,expected.size());
    gpu().flush(true);
    void* mapped=nullptr;D3D12_RANGE range{0,expected.size()};
    check(readback->object->Map(0,&range,&mapped),"Map verification readback");
    const bool same=!std::memcmp(mapped,expected.data(),expected.size());
    D3D12_RANGE written{0,0};readback->object->Unmap(0,&written);
    require(same,"Uploaded data differs from the CPU buffer snapshot.");
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"Pass the test report directory.");
        require(mxl::diag::start(nullptr,argv[1]),"Start test diagnostics.");
        state().device=std::make_unique<Device>();
        constexpr size_t Draws=150,Chunk=16*1024,Used=Draws*Chunk;
        std::vector<uint8_t> bytes(Used);for(size_t i=0;i<Used;++i)bytes[i]=uint8_t(i*19+7);
        GLuint id=0;glGenBuffers(1,&id);glBindBuffer(GL_ARRAY_BUFFER,id);
        glBufferData(GL_ARRAY_BUFFER,Used*2,nullptr,GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER,0,Used,bytes.data());
        auto& buffer=state().buffers.at(id);
        double baseline_ms=0,fixed_ms=0;
        for(int mode=0;mode<2;++mode) {
            // A zero hint reproduces beta 1's growing-prefix upload algorithm.
            buffer.prefetch_bytes=mode?Used:0;
            mxl::diag::begin_frame(mode+1,0,1280,720,true);
            const auto began=mxl::diag::ticks();Upload first{},last{};
            for(size_t draw=1;draw<=Draws;++draw) {
                last=upload_buffer(buffer,draw*Chunk);
                if(draw==1)first=last;
                if(mode)require(last.resource==first.resource && last.offset==first.offset,"A later draw reuploaded an unchanged buffer.");
            }
            const auto elapsed=mxl::diag::milliseconds(mxl::diag::ticks()-began);
            if(mode)fixed_ms=elapsed;else baseline_ms=elapsed;
            verify_upload(last,bytes);mxl::diag::end_frame();
        }
        // A partial update at zero is only a hint. Unmodified tail bytes must
        // remain available if a later draw needs more than that hint.
        glBindBuffer(GL_ARRAY_BUFFER,id);std::fill(bytes.begin(),bytes.begin()+64,0xA6);
        glBufferSubData(GL_ARRAY_BUFFER,0,64,bytes.data());
        auto short_upload=upload_buffer(buffer,64);require(buffer.upload_size==64,"Small update over-uploaded.");
        auto whole=upload_buffer(buffer,Used);require(whole.resource!=short_upload.resource || whole.offset!=short_upload.offset,"Required tail did not extend upload.");
        verify_upload(whole,bytes);
        std::fill(bytes.begin()+256,bytes.begin()+384,0xB7);
        glBufferSubData(GL_ARRAY_BUFFER,256,128,bytes.data()+256);
        require(buffer.prefetch_bytes==384,"Offset update extent incorrect.");
        verify_upload(upload_buffer(buffer,Used),bytes);
        glBufferData(GL_ARRAY_BUFFER,512,nullptr,GL_DYNAMIC_DRAW);
        require(buffer.prefetch_bytes==0,"Reallocated buffer retained old hint.");
        bool rejected=false;try{upload_buffer(buffer,513);}catch(const std::exception&){rejected=true;}
        require(rejected,"Out-of-bounds request was accepted.");
        gpu().wait_idle();mxl::diag::stop(true);
        std::cout<<"Growing-prefix workload: "<<Draws<<" draws, "<<Used<<" useful bytes.\n"
                 <<"Beta 1 behavior: "<<(Chunk*Draws*(Draws+1)/2)<<" copied bytes, "<<baseline_ms<<" ms.\n"
                 <<"Beta 2 behavior: "<<Used<<" copied bytes, "<<fixed_ms<<" ms.\n"
                 <<"PASS: one upload per unchanged frame/version, GPU byte accuracy, partial-update tail preservation and bounds.\n";
        return 0;
    }catch(const std::exception& error){mxl::diag::stop(true);std::cerr<<error.what()<<"\n";return 1;}
}
