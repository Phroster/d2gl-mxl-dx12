#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

namespace mxl::dx12 {
using Microsoft::WRL::ComPtr;
void check(HRESULT hr,const char* operation);
struct Resource {
    ComPtr<ID3D12Resource> object;
    D3D12_RESOURCE_STATES state=D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_DESC desc{};
};
struct Upload {
    ID3D12Resource* resource=nullptr;
    uint64_t offset=0;
    uint8_t* cpu=nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu=0;
};
struct DescriptorTable {
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
    uint32_t increment=0, count=0;
    D3D12_CPU_DESCRIPTOR_HANDLE at(uint32_t i) const { return {cpu.ptr+SIZE_T(i)*increment}; }
};
class Device {
public:
    static constexpr uint32_t FrameCount=3;
    explicit Device(HWND window=nullptr,bool debug=false);
    ~Device();
    Device(const Device&)=delete;
    Device& operator=(const Device&)=delete;
    ID3D12Device* native() const { return device_.Get(); }
    ID3D12GraphicsCommandList* commands();
    ID3D12CommandQueue* queue() const { return queue_.Get(); }
    std::string adapter_name() const { return adapter_name_; }
    uint32_t frame_number() const { return frame_index_; }
    uint64_t frame_serial() const { return frame_serial_; }
    bool debug_enabled() const { return debug_enabled_; }
    std::shared_ptr<Resource> texture(uint32_t width,uint32_t height,uint32_t layers,DXGI_FORMAT format,
                                      D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,uint32_t levels=1);
    std::shared_ptr<Resource> buffer(uint64_t size,D3D12_HEAP_TYPE heap=D3D12_HEAP_TYPE_DEFAULT);
    void transition(Resource& resource,D3D12_RESOURCE_STATES state);
    Upload upload(uint64_t size,uint64_t alignment=256);
    DescriptorTable descriptors(uint32_t count,bool sampler=false);
    void bind_descriptor_heaps();
    void keep_alive(const std::shared_ptr<Resource>& resource);
    void upload_texture(Resource& destination,uint32_t layer,uint32_t x,uint32_t y,
                        uint32_t width,uint32_t height,const uint8_t* pixels,uint32_t bytes_per_pixel);
    std::vector<uint8_t> readback(Resource& resource,uint32_t bytes_per_pixel);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv(Resource& resource);
    void release_rtv(Resource& resource);
    Resource& back_buffer();
    void resize(uint32_t width,uint32_t height);
    void present(bool vsync);
    void flush(bool wait=true);
    void wait_idle();
    uint32_t validation_errors() const;
private:
    static constexpr uint64_t UploadBytes=64ull*1024*1024;
    struct Frame {
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12DescriptorHeap> views,samplers;
        ComPtr<ID3D12Resource> upload;
        uint8_t* mapped=nullptr;
        uint64_t used=0, fence=0;
        uint32_t view_count=0,sampler_count=0;
        std::vector<std::shared_ptr<Resource>> keepalive;
        std::unordered_set<Resource*> tracked;
    };
    void begin();
    void wait_for(uint64_t value);
    void create_back_buffers();
    ComPtr<ID3D12Device> device_;
    ComPtr<IDXGIFactory6> factory_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12GraphicsCommandList> list_;
    ComPtr<ID3D12Fence> fence_;
    ComPtr<IDXGISwapChain3> swap_;
    ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    std::array<Frame,FrameCount> frames_;
    std::array<Resource,FrameCount> backbuffers_;
    std::vector<std::pair<ID3D12Resource*,uint32_t>> rtvs_;
    std::vector<uint32_t> free_rtvs_;
    HANDLE fence_event_=nullptr,latency_event_=nullptr;
    uint64_t fence_value_=0;
    uint64_t frame_serial_=0;
    uint32_t frame_index_=0,rtv_count_=FrameCount;
    uint32_t view_increment_=0,sampler_increment_=0,rtv_increment_=0;
    uint32_t width_=1,height_=1;
    bool active_=false,tearing_=false,debug_enabled_=false;
    std::string adapter_name_;
};
}
