#include "device.h"
#include "fps_menu.h"
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx12.h>
#include <iostream>
#include <filesystem>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
int main() {
    HWND window=nullptr;
    try {
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"MXL_DX12_HiddenTest";
        RegisterClassW(&wc);window=CreateWindowExW(0,wc.lpszClassName,L"DX12 test",WS_POPUP,0,0,800,600,nullptr,nullptr,wc.hInstance,nullptr);
        if(!window)throw std::runtime_error("Test window creation failed");
        mxl::dx12::Device device(window,false);
        D3D12_DESCRIPTOR_HEAP_DESC desc{};desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;desc.NumDescriptors=1;desc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        mxl::dx12::ComPtr<ID3D12DescriptorHeap> heap;mxl::dx12::check(device.native()->CreateDescriptorHeap(&desc,IID_PPV_ARGS(&heap)),"Create UI heap");
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={800,600};io.DeltaTime=1.0f/144;io.IniFilename=nullptr;
        io.Fonts->AddFontDefault();
        if(!ImGui_ImplDX12_Init(device.native(),3,DXGI_FORMAT_R8G8B8A8_UNORM,heap.Get(),heap->GetCPUDescriptorHandleForHeapStart(),heap->GetGPUDescriptorHandleForHeapStart()))throw std::runtime_error("UI backend failed");
        ImGui_ImplDX12_NewFrame();ImGui::NewFrame();
        ImGui::SetNextWindowPos({60,30});ImGui::SetNextWindowSize({680,540});
        ImGui::Begin("MXL Smooth Motion - DX12 Experiment",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
        ImGui::TextColored({0.9f,0.65f,0.3f,1},"Native DirectX 12 / NVIDIA hardware");
        ImGui::Separator();mxl::dx12::draw_fps_settings();ImGui::End();ImGui::Render();
        auto& output=device.back_buffer();device.transition(output,D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float clear[]={0.04f,0.04f,0.05f,1};auto rtv=device.rtv(output);auto* cmd=device.commands();
        cmd->ClearRenderTargetView(rtv,clear,0,nullptr);cmd->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
        ID3D12DescriptorHeap* heaps[]={heap.Get()};cmd->SetDescriptorHeaps(1,heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),cmd);
        auto bytes=device.readback(output,4);size_t bright=0;
        for(size_t i=0;i<bytes.size();i+=4)if(bytes[i]>100&&bytes[i+1]>100&&bytes[i+2]>100)++bright;
        if(bright<500)throw std::runtime_error("Menu text did not render.");
        auto path=std::filesystem::path(MXL_BUILD_DIR)/"dx12-menu-preview.png";
        if(!stbi_write_png(path.string().c_str(),800,600,4,bytes.data(),800*4))throw std::runtime_error("Could not save UI test image.");
        device.wait_idle();ImGui_ImplDX12_Shutdown();ImGui::DestroyContext();
        std::cout<<"PASS: DX12 Dear ImGui font rendering and D2FPS options panel; "<<bright<<" visible text pixels.\n";
        DestroyWindow(window);
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";if(window)DestroyWindow(window);return 1;}
}
