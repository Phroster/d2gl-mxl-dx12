#include "device.h"
#include <iostream>
int main() {
    try {
        mxl::dx12::Device device(nullptr,true);
        std::cout<<"Hardware adapter: "<<device.adapter_name()<<"\n";
        auto texture=device.texture(17,13,1,DXGI_FORMAT_R8G8B8A8_UNORM);
        const float color[]={1.0f,0.0f,0.0f,1.0f};
        device.transition(*texture,D3D12_RESOURCE_STATE_RENDER_TARGET);
        device.commands()->ClearRenderTargetView(device.rtv(*texture),color,0,nullptr);
        auto bytes=device.readback(*texture,4);
        for(size_t i=0;i<bytes.size();i+=4)
            if(bytes[i]!=255 || bytes[i+1] || bytes[i+2] || bytes[i+3]!=255) return 2;
        std::vector<uint8_t> pattern(17*13*4);
        for(size_t i=0;i<pattern.size();++i) pattern[i]=uint8_t(i*17+3);
        device.upload_texture(*texture,0,0,0,17,13,pattern.data(),4);
        if(device.readback(*texture,4)!=pattern) return 3;
        device.wait_idle();
        if(device.validation_errors()) return 4;
        std::cout<<"PASS: native DX12 render-target clear and padded texture upload/readback. Debug layer "
                 <<(device.debug_enabled()?"enabled":"unavailable")<<".\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
