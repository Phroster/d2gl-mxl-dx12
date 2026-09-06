#include "device.h"
#include <d3d12sdklayers.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <cstdio>

namespace mxl::dx12 {
void check(HRESULT hr,const char* operation) {
    if(FAILED(hr)) {
        char msg[256]; sprintf_s(msg,"%s failed (0x%08lx)",operation,static_cast<unsigned long>(hr));
        throw std::runtime_error(msg);
    }
}
static D3D12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES p{};p.Type=type;p.CreationNodeMask=1;p.VisibleNodeMask=1;return p;
}
static D3D12_RESOURCE_DESC buffer_desc(uint64_t size) {
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=size;
    d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return d;
}
Device::Device(HWND window,bool debug) {
    if(debug) {
        ComPtr<ID3D12Debug> layer;
        if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&layer)))) { layer->EnableDebugLayer();debug_enabled_=true; }
    }
    check(CreateDXGIFactory2(debug_enabled_?DXGI_CREATE_FACTORY_DEBUG:0,IID_PPV_ARGS(&factory_)),"CreateDXGIFactory2");
    ComPtr<IDXGIAdapter1> adapter;
    for(UINT i=0;factory_->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;++i) {
        DXGI_ADAPTER_DESC1 info{};adapter->GetDesc1(&info);
        if(!(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) && SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device_)))) {
            char text[256];WideCharToMultiByte(CP_UTF8,0,info.Description,-1,text,sizeof(text),nullptr,nullptr);adapter_name_=text;break;
        }
        adapter.Reset();
    }
    if(!device_) throw std::runtime_error("No hardware Direct3D 12 adapter is available.");
    D3D12_COMMAND_QUEUE_DESC queue_desc{};queue_desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    check(device_->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&queue_)),"CreateCommandQueue");
    check(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_)),"CreateFence");
    fence_event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!fence_event_) throw std::runtime_error("CreateEvent failed.");
    view_increment_=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    sampler_increment_=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    rtv_increment_=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=4096;
    check(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv_heap_)),"Create RTV heap");
    for(auto& f:frames_) {
        check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&f.allocator)),"CreateCommandAllocator");
        hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=65536;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        check(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&f.views)),"Create shader resource heap");
        hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;hd.NumDescriptors=2048;
        check(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&f.samplers)),"Create sampler heap");
        auto desc=buffer_desc(UploadBytes);auto heap=heap_properties(D3D12_HEAP_TYPE_UPLOAD);
        check(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&f.upload)),"Create upload arena");
        D3D12_RANGE none{0,0};
        check(f.upload->Map(0,&none,reinterpret_cast<void**>(&f.mapped)),"Map upload arena");
    }
    check(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,frames_[0].allocator.Get(),nullptr,IID_PPV_ARGS(&list_)),"CreateCommandList");
    check(list_->Close(),"Close initial command list");
    if(window) {
        RECT rc{};GetClientRect(window,&rc);width_=std::max(1L,rc.right);height_=std::max(1L,rc.bottom);
        BOOL allowed=FALSE;factory_->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,&allowed,sizeof(allowed));tearing_=allowed!=FALSE;
        DXGI_SWAP_CHAIN_DESC1 d{};d.Width=width_;d.Height=height_;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count=1;d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.BufferCount=FrameCount;
        d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;d.Scaling=DXGI_SCALING_STRETCH;
        d.Flags=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT|(tearing_?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0);
        ComPtr<IDXGISwapChain1> sc;
        check(factory_->CreateSwapChainForHwnd(queue_.Get(),window,&d,nullptr,nullptr,&sc),"Create DX12 swap chain");
        check(sc.As(&swap_),"Query swap chain");
        factory_->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER);
        swap_->SetMaximumFrameLatency(1);latency_event_=swap_->GetFrameLatencyWaitableObject();
        create_back_buffers();
    }
}
Device::~Device() {
    try { wait_idle(); } catch(...) {}
    if(latency_event_) CloseHandle(latency_event_);
    if(fence_event_) CloseHandle(fence_event_);
    for(auto& f:frames_) if(f.mapped) f.upload->Unmap(0,nullptr);
}
void Device::wait_for(uint64_t value) {
    if(fence_->GetCompletedValue()<value) {
        check(fence_->SetEventOnCompletion(value,fence_event_),"SetEventOnCompletion");
        if(WaitForSingleObject(fence_event_,10000)!=WAIT_OBJECT_0) throw std::runtime_error("DX12 fence timed out.");
    }
}
void Device::begin() {
    if(active_) return;
    auto& f=frames_[frame_index_];wait_for(f.fence);
    f.keepalive.clear();f.tracked.clear();f.used=0;f.view_count=0;f.sampler_count=0;
    check(f.allocator->Reset(),"Reset command allocator");
    check(list_->Reset(f.allocator.Get(),nullptr),"Reset command list");
    active_=true;
}
ID3D12GraphicsCommandList* Device::commands() { begin();return list_.Get(); }
void Device::flush(bool wait) {
    if(!active_) return;
    check(list_->Close(),"Close command list");
    ID3D12CommandList* lists[]={list_.Get()};queue_->ExecuteCommandLists(1,lists);
    auto& f=frames_[frame_index_];f.fence=++fence_value_;check(queue_->Signal(fence_.Get(),f.fence),"Signal fence");
    active_=false;
    if(wait) wait_for(f.fence);
    frame_index_=(frame_index_+1)%FrameCount;
    ++frame_serial_;
}
void Device::wait_idle() {
    flush(false);
    const auto value=++fence_value_;check(queue_->Signal(fence_.Get(),value),"Signal idle fence");wait_for(value);
}
std::shared_ptr<Resource> Device::buffer(uint64_t size,D3D12_HEAP_TYPE heap_type) {
    auto r=std::make_shared<Resource>();r->desc=buffer_desc(size);
    auto heap=heap_properties(heap_type);
    r->state=heap_type==D3D12_HEAP_TYPE_READBACK?D3D12_RESOURCE_STATE_COPY_DEST:heap_type==D3D12_HEAP_TYPE_UPLOAD?D3D12_RESOURCE_STATE_GENERIC_READ:D3D12_RESOURCE_STATE_COMMON;
    check(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&r->desc,r->state,nullptr,IID_PPV_ARGS(&r->object)),"Create buffer");
    return r;
}
std::shared_ptr<Resource> Device::texture(uint32_t width,uint32_t height,uint32_t layers,DXGI_FORMAT format,D3D12_RESOURCE_FLAGS flags,uint32_t levels) {
    auto r=std::make_shared<Resource>();
    auto& d=r->desc;d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width;d.Height=height;
    d.DepthOrArraySize=static_cast<UINT16>(layers);d.MipLevels=static_cast<UINT16>(levels);d.Format=format;d.SampleDesc.Count=1;d.Flags=flags;
    auto heap=heap_properties(D3D12_HEAP_TYPE_DEFAULT);
    check(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,r->state,nullptr,IID_PPV_ARGS(&r->object)),"Create texture");
    return r;
}
void Device::transition(Resource& r,D3D12_RESOURCE_STATES state) {
    if(r.state==state) return;
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource=r.object.Get();b.Transition.StateBefore=r.state;b.Transition.StateAfter=state;
    b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commands()->ResourceBarrier(1,&b);r.state=state;
}
Upload Device::upload(uint64_t size,uint64_t alignment) {
    begin();auto& f=frames_[frame_index_];const auto offset=(f.used+alignment-1)&~(alignment-1);
    if(offset+size>UploadBytes) {
        // Asset loading can exceed one arena before the first frame is submitted.
        // A fenced spill allocation keeps that case correct without a GPU stall.
        auto spill=buffer((size+alignment-1)&~(alignment-1),D3D12_HEAP_TYPE_UPLOAD);
        uint8_t* mapped=nullptr;D3D12_RANGE none{0,0};
        check(spill->object->Map(0,&none,reinterpret_cast<void**>(&mapped)),"Map upload spill");
        keep_alive(spill);
        return {spill->object.Get(),0,mapped,spill->object->GetGPUVirtualAddress()};
    }
    f.used=offset+size;return {f.upload.Get(),offset,f.mapped+offset,f.upload->GetGPUVirtualAddress()+offset};
}
DescriptorTable Device::descriptors(uint32_t count,bool sampler) {
    begin();auto& f=frames_[frame_index_];auto& used=sampler?f.sampler_count:f.view_count;
    const auto max=sampler?2048u:65536u;if(count>max-used) throw std::runtime_error("DX12 descriptor heap exhausted.");
    auto* heap=sampler?f.samplers.Get():f.views.Get();const auto stride=sampler?sampler_increment_:view_increment_;
    DescriptorTable t{heap->GetCPUDescriptorHandleForHeapStart(),heap->GetGPUDescriptorHandleForHeapStart(),stride,count};
    t.cpu.ptr+=SIZE_T(used)*stride;t.gpu.ptr+=UINT64(used)*stride;used+=count;return t;
}
void Device::bind_descriptor_heaps() {
    begin();auto& f=frames_[frame_index_];ID3D12DescriptorHeap* heaps[]={f.views.Get(),f.samplers.Get()};list_->SetDescriptorHeaps(2,heaps);
}
void Device::keep_alive(const std::shared_ptr<Resource>& r) {
    begin();auto& frame=frames_[frame_index_];
    if(frame.tracked.insert(r.get()).second)frame.keepalive.push_back(r);
}
void Device::upload_texture(Resource& r,uint32_t layer,uint32_t x,uint32_t y,uint32_t width,uint32_t height,const uint8_t* pixels,uint32_t bpp) {
    const uint32_t pitch=(width*bpp+255)&~255u;auto up=upload(uint64_t(pitch)*height,512);
    for(uint32_t row=0;row<height;++row) std::memcpy(up.cpu+row*pitch,pixels+uint64_t(row)*width*bpp,width*bpp);
    transition(r,D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=up.resource;src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset=up.offset;src.PlacedFootprint.Footprint={r.desc.Format,width,height,1,pitch};
    D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=r.object.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=layer*r.desc.MipLevels;
    commands()->CopyTextureRegion(&dst,x,y,0,&src,nullptr);
}
std::vector<uint8_t> Device::readback(Resource& r,uint32_t bpp) {
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT rows=0;UINT64 row_size=0,bytes=0;
    device_->GetCopyableFootprints(&r.desc,0,1,0,&fp,&rows,&row_size,&bytes);
    auto rb=buffer(bytes,D3D12_HEAP_TYPE_READBACK);transition(r,D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=r.object.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=rb->object.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=fp;
    commands()->CopyTextureRegion(&dst,0,0,0,&src,nullptr);flush(true);
    std::vector<uint8_t> output(size_t(r.desc.Width)*r.desc.Height*bpp);
    uint8_t* mapped=nullptr;D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};
    check(rb->object->Map(0,&range,reinterpret_cast<void**>(&mapped)),"Map readback");
    for(uint32_t y=0;y<r.desc.Height;++y) std::memcpy(output.data()+y*r.desc.Width*bpp,mapped+fp.Offset+y*fp.Footprint.RowPitch,r.desc.Width*bpp);
    D3D12_RANGE written{0,0};rb->object->Unmap(0,&written);return output;
}
D3D12_CPU_DESCRIPTOR_HANDLE Device::rtv(Resource& r) {
    uint32_t slot=UINT32_MAX;
    for(auto [resource,index]:rtvs_) if(resource==r.object.Get()) {slot=index;break;}
    if(slot==UINT32_MAX) {
        if(!free_rtvs_.empty()) {slot=free_rtvs_.back();free_rtvs_.pop_back();}
        else {if(rtv_count_==4096) throw std::runtime_error("RTV heap exhausted.");slot=rtv_count_++;}
        rtvs_.push_back({r.object.Get(),slot});
        auto handle=rtv_heap_->GetCPUDescriptorHandleForHeapStart();handle.ptr+=SIZE_T(slot)*rtv_increment_;
        D3D12_RENDER_TARGET_VIEW_DESC desc{};desc.Format=r.desc.Format;desc.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;
        device_->CreateRenderTargetView(r.object.Get(),&desc,handle);
    }
    auto handle=rtv_heap_->GetCPUDescriptorHandleForHeapStart();handle.ptr+=SIZE_T(slot)*rtv_increment_;return handle;
}
void Device::release_rtv(Resource& r) {
    for(auto it=rtvs_.begin();it!=rtvs_.end();++it) if(it->first==r.object.Get()) {free_rtvs_.push_back(it->second);rtvs_.erase(it);break;}
}
void Device::create_back_buffers() {
    for(uint32_t i=0;i<FrameCount;++i) {
        check(swap_->GetBuffer(i,IID_PPV_ARGS(&backbuffers_[i].object)),"Get back buffer");
        backbuffers_[i].desc=backbuffers_[i].object->GetDesc();backbuffers_[i].state=D3D12_RESOURCE_STATE_PRESENT;
    }
}
Resource& Device::back_buffer() {
    if(!swap_) throw std::runtime_error("No swap chain in offscreen test.");
    return backbuffers_[swap_->GetCurrentBackBufferIndex()];
}
void Device::resize(uint32_t width,uint32_t height) {
    if(!swap_ || !width || !height || (width==width_&&height==height_)) return;
    wait_idle();
    for(auto& f:frames_) f.keepalive.clear();
    for(auto& b:backbuffers_) {release_rtv(b);b.object.Reset();}
    const UINT flags=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT|(tearing_?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0);
    check(swap_->ResizeBuffers(FrameCount,width,height,DXGI_FORMAT_R8G8B8A8_UNORM,flags),"ResizeBuffers");
    width_=width;height_=height;create_back_buffers();
}
void Device::present(bool vsync) {
    if(!swap_) {flush(false);return;}
    transition(back_buffer(),D3D12_RESOURCE_STATE_PRESENT);
    flush(false);
    const auto result=swap_->Present(vsync?1:0,(!vsync&&tearing_)?DXGI_PRESENT_ALLOW_TEARING:0);
    check(result,"Present");
    if(result==DXGI_STATUS_OCCLUDED){Sleep(10);return;}
    if(latency_event_ && WaitForSingleObject(latency_event_,1000)==WAIT_FAILED) throw std::runtime_error("Frame latency wait failed.");
}
uint32_t Device::validation_errors() const {
    ComPtr<ID3D12InfoQueue> info;if(FAILED(device_.As(&info))) return 0;
    uint32_t errors=0;
    for(UINT64 i=0;i<info->GetNumStoredMessagesAllowedByRetrievalFilter();++i) {
        SIZE_T size=0;info->GetMessage(i,nullptr,&size);std::vector<uint8_t> bytes(size);
        auto* msg=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());info->GetMessage(i,msg,&size);
        if(msg->Severity<=D3D12_MESSAGE_SEVERITY_ERROR) {++errors;OutputDebugStringA(msg->pDescription);}
    }
    return errors;
}
}
