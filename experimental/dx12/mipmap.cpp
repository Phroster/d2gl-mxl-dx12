#include "backend_state.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace mxl::dx12 {
void generate_mips(TextureState& texture) {
    if(!texture.mip_requested||!texture.mips_dirty)return;
    auto old=texture.resource;uint32_t count=1;
    for(uint64_t n=std::max(old->desc.Width,uint64_t(old->desc.Height));n>1;n>>=1)++count;
    if(count==1){texture.mips_dirty=false;return;}
    if(old->desc.MipLevels!=count) {
        auto full=gpu().texture(UINT(old->desc.Width),old->desc.Height,old->desc.DepthOrArraySize,old->desc.Format,
            D3D12_RESOURCE_FLAGS(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET|D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),count);
        gpu().transition(*old,D3D12_RESOURCE_STATE_COPY_SOURCE);gpu().transition(*full,D3D12_RESOURCE_STATE_COPY_DEST);
        for(UINT layer=0;layer<old->desc.DepthOrArraySize;++layer) {
            D3D12_TEXTURE_COPY_LOCATION src{},dst{};
            src.pResource=old->object.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;src.SubresourceIndex=layer*old->desc.MipLevels;
            dst.pResource=full->object.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=layer*count;
            gpu().commands()->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
        }
        gpu().keep_alive(old);gpu().release_rtv(*old);texture.resource=full;
    }
    auto& r=*texture.resource;
    // The module lives as long as the process; its pipeline remains valid for in-flight frames.
    static ComPtr<ID3D12RootSignature> root;
    static ComPtr<ID3D12PipelineState> pipeline;
    if(!root) {
        const char* shader=R"(
Texture2DArray<float4> Source:register(t0);
RWTexture2DArray<float4> Destination:register(u0);
[numthreads(8,8,1)] void main(uint3 p:SV_DispatchThreadID) {
 uint w,h,l;Destination.GetDimensions(w,h,l);if(p.x>=w||p.y>=h||p.z>=l)return;
 uint sw,sh,sl,sm;Source.GetDimensions(0,sw,sh,sl,sm);
 uint2 a=p.xy*2;float4 sum=0;
 [unroll] for(uint y=0;y<2;y++)[unroll] for(uint x=0;x<2;x++)
   sum+=Source.Load(int4(min(a+uint2(x,y),uint2(sw-1,sh-1)),p.z,0));
 Destination[p]=sum*0.25;
})";
        ComPtr<ID3DBlob> code,error;check(D3DCompile(shader,strlen(shader),"DX12 mipmaps",nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error),"Compile mipmap shader");
        D3D12_DESCRIPTOR_RANGE ranges[]={{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
        D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;param.DescriptorTable={2,ranges};
        D3D12_ROOT_SIGNATURE_DESC desc{};desc.NumParameters=1;desc.pParameters=&param;
        ComPtr<ID3DBlob> blob;check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error),"Serialize mipmap root");
        check(gpu().native()->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)),"Create mipmap root");
        D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};pso.pRootSignature=root.Get();pso.CS={code->GetBufferPointer(),code->GetBufferSize()};
        check(gpu().native()->CreateComputePipelineState(&pso,IID_PPV_ARGS(&pipeline)),"Create mipmap pipeline");
    }
    gpu().transition(r,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    auto* cmd=gpu().commands();gpu().bind_descriptor_heaps();cmd->SetPipelineState(pipeline.Get());cmd->SetComputeRootSignature(root.Get());
    for(UINT mip=1;mip<count;++mip) {
        auto views=gpu().descriptors(2);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=r.desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2DArray={mip-1,1,0,r.desc.DepthOrArraySize,0,0};
        gpu().native()->CreateShaderResourceView(r.object.Get(),&srv,views.at(0));
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=r.desc.Format;uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        uav.Texture2DArray={mip,0,r.desc.DepthOrArraySize,0};gpu().native()->CreateUnorderedAccessView(r.object.Get(),nullptr,&uav,views.at(1));
        for(UINT layer=0;layer<r.desc.DepthOrArraySize;++layer) {
            D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r.object.Get(),mip+layer*count,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS};cmd->ResourceBarrier(1,&b);
        }
        cmd->SetComputeRootDescriptorTable(0,views.gpu);
        cmd->Dispatch((std::max(1u,UINT(r.desc.Width)>>mip)+7)/8,(std::max(1u,r.desc.Height>>mip)+7)/8,r.desc.DepthOrArraySize);
        for(UINT layer=0;layer<r.desc.DepthOrArraySize;++layer) {
            D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r.object.Get(),mip+layer*count,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};cmd->ResourceBarrier(1,&b);
        }
    }
    texture.mips_dirty=false;gpu().keep_alive(texture.resource);
}
}
