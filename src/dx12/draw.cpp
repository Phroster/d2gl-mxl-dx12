#include "backend_state.h"
#include <d3dcompiler.h>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include "diagnostics.h"

namespace mxl::dx12 {
static constexpr uint32_t CBCount=16, TexCount=32, ImageCount=16, ViewCount=CBCount+TexCount+ImageCount;
template<class T> static void key_append(std::string& key,const T& value) {key.append(reinterpret_cast<const char*>(std::addressof(value)),sizeof(value));}
static ID3D12RootSignature* root_signature(bool compute) {
    auto& s=state();auto& root=compute?s.compute_root:s.graphics_root;if(root)return root.Get();
    D3D12_DESCRIPTOR_RANGE views[3]{};
    views[0]={D3D12_DESCRIPTOR_RANGE_TYPE_CBV,CBCount,0,0,0};
    views[1]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,TexCount,0,0,CBCount};
    views[2]={D3D12_DESCRIPTOR_RANGE_TYPE_UAV,ImageCount,0,0,CBCount+TexCount};
    D3D12_DESCRIPTOR_RANGE samplers{D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,TexCount,0,0,0};
    D3D12_ROOT_PARAMETER params[4]{};
    for(uint32_t i=0;i<(compute?2u:4u);++i) {
        params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[i].DescriptorTable={(i%2)?1u:3u,(i%2)?&samplers:views};
        params[i].ShaderVisibility=compute?D3D12_SHADER_VISIBILITY_ALL:i<2?D3D12_SHADER_VISIBILITY_VERTEX:D3D12_SHADER_VISIBILITY_PIXEL;
    }
    D3D12_ROOT_SIGNATURE_DESC desc{};desc.NumParameters=compute?2:4;desc.pParameters=params;
    desc.Flags=compute?D3D12_ROOT_SIGNATURE_FLAG_NONE:D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> blob,error;
    check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error),"Serialize root signature");
    check(gpu().native()->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)),"Create root signature");
    return root.Get();
}
static DXGI_FORMAT attribute_format(const Attribute& a) {
    if(a.type==GL_FLOAT) {
        if(a.components==2)return DXGI_FORMAT_R32G32_FLOAT;
        if(a.components==3)return DXGI_FORMAT_R32G32B32_FLOAT;
        if(a.components==4)return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    if(a.type==GL_HALF_FLOAT&&a.components==2)return DXGI_FORMAT_R16G16_FLOAT;
    if(a.type==GL_UNSIGNED_BYTE&&a.components==4)return a.normalized?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_R8G8B8A8_UINT;
    if(a.type==GL_UNSIGNED_SHORT&&a.components==2)return a.integer?DXGI_FORMAT_R16G16_UINT:DXGI_FORMAT_R16G16_UNORM;
    throw std::runtime_error("Unsupported DX12 vertex format");
}
static D3D12_BLEND blend_factor(GLenum x) {
    switch(x) {case GL_ZERO:return D3D12_BLEND_ZERO;case GL_ONE:return D3D12_BLEND_ONE;
        case GL_SRC_COLOR:return D3D12_BLEND_SRC_COLOR;case GL_SRC_ALPHA:return D3D12_BLEND_SRC_ALPHA;
        case GL_ONE_MINUS_SRC_ALPHA:return D3D12_BLEND_INV_SRC_ALPHA;default:throw std::runtime_error("Unsupported DX12 blend factor");}
}
static uint32_t uniform_slot(const ProgramState& p,const std::string& name) {
    auto it=p.values.find(name);if(it==p.values.end()||it->second.size()<4)return 0;
    uint32_t value;memcpy(&value,it->second.data(),4);return value;
}
static void prepare_textures(const ShaderCode& code,const ProgramState& p) {
    for(const auto& r:code.resources)if(r.kind==ShaderResource::Kind::Texture) {
        const auto id=state().texture_slots.at(uniform_slot(p,r.name));if(id)generate_mips(state().textures.at(id));
    }
}
static std::vector<uint8_t> constants(const ProgramState& p,const ShaderResource& r) {
    std::vector<uint8_t> bytes((r.byte_size+255)&~255u,0);
    if(r.globals) {
        for(const auto& member:r.members) {
            auto it=p.values.find(member.name);
            if(it!=p.values.end()&&member.offset+member.size<=bytes.size())
                memcpy(bytes.data()+member.offset,it->second.data(),std::min(size_t(member.size),it->second.size()));
        }
    } else {
        auto bind=p.block_bindings.find(r.name);
        if(bind!=p.block_bindings.end()) {
            auto u=state().ubos.find(bind->second);
            if(u!=state().ubos.end()&&u->second.buffer) {
                auto& input=state().buffers.at(u->second.buffer).bytes;
                if(uint64_t(u->second.offset)+u->second.size>input.size())throw std::runtime_error("Uniform buffer binding out of bounds");
                memcpy(bytes.data(),input.data()+u->second.offset,std::min(size_t(u->second.size),bytes.size()));
            }
        }
    }
    return bytes;
}
static D3D12_TEXTURE_ADDRESS_MODE address(GLint mode) {
    switch(mode) {case GL_REPEAT:return D3D12_TEXTURE_ADDRESS_MODE_WRAP;case GL_MIRRORED_REPEAT:return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case GL_CLAMP_TO_BORDER:return D3D12_TEXTURE_ADDRESS_MODE_BORDER;default:return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;}
}
static D3D12_SAMPLER_DESC sampler(const TextureState* texture) {
    D3D12_SAMPLER_DESC s{};s.Filter=texture&&(texture->filter_mag==GL_LINEAR||texture->filter_min==GL_LINEAR)?D3D12_FILTER_MIN_MAG_MIP_LINEAR:D3D12_FILTER_MIN_MAG_MIP_POINT;
    s.AddressU=texture?address(texture->wrap_s):D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s.AddressV=texture?address(texture->wrap_t):D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    s.MaxAnisotropy=1;s.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;s.MinLOD=0;s.MaxLOD=D3D12_FLOAT32_MAX;return s;
}
static void bind_resources(const ShaderCode& code,ProgramState& program,uint32_t root_index,bool compute) {
    diag::Scope measured(diag::Metric::Bindings);
    auto& s=state();
    if(s.sampler_frame!=gpu().frame_serial()) {s.sampler_frame=gpu().frame_serial();s.sampler_tables.clear();s.binding_tables.clear();}
    std::string key;key_append(key,s.program);key_append(key,root_index);key_append(key,compute);key_append(key,program.version);
    for(const auto& r:code.resources) {
        if(r.kind==ShaderResource::Kind::Constants&&!r.globals) {
            auto it=program.block_bindings.find(r.name);if(it!=program.block_bindings.end()) {
                auto u=s.ubos.find(it->second);if(u!=s.ubos.end()) {key_append(key,u->second);
                    if(u->second.buffer)key_append(key,s.buffers.at(u->second.buffer).version);}
            }
        } else if(r.kind!=ShaderResource::Kind::Constants) {
            auto slot=uniform_slot(program,r.name);GLuint id=r.kind==ShaderResource::Kind::Texture?s.texture_slots.at(slot):s.image_slots.at(slot);
            key_append(key,id);
            if(id){auto& t=s.textures.at(id);key_append(key,t.resource->object);key_append(key,t.filter_min);key_append(key,t.filter_mag);key_append(key,t.wrap_s);key_append(key,t.wrap_t);}
        }
    }
    // Transition on every use even when a descriptor table can be reused.
    for(const auto& r:code.resources) if(r.kind!=ShaderResource::Kind::Constants) {
        auto slot=uniform_slot(program,r.name);
        auto id=r.kind==ShaderResource::Kind::Texture?s.texture_slots.at(slot):s.image_slots.at(slot);
        if(id){auto& t=s.textures.at(id);gpu().transition(*t.resource,r.kind==ShaderResource::Kind::Texture?
            D3D12_RESOURCE_STATES(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE):D3D12_RESOURCE_STATE_UNORDERED_ACCESS);gpu().keep_alive(t.resource);}
    }
    auto found=s.binding_tables.find(key);
    if(found==s.binding_tables.end()) {
        diag::count(diag::Count::BindingMisses);
        auto views=gpu().descriptors(ViewCount);
        auto zero=gpu().upload(256);memset(zero.cpu,0,256);
        D3D12_CONSTANT_BUFFER_VIEW_DESC null_cb{zero.gpu,256};
        for(uint32_t i=0;i<CBCount;++i)gpu().native()->CreateConstantBufferView(&null_cb,views.at(i));
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
        for(uint32_t i=0;i<TexCount;++i)gpu().native()->CreateShaderResourceView(nullptr,&srv,views.at(CBCount+i));
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R8G8B8A8_UNORM;uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
        for(uint32_t i=0;i<ImageCount;++i)gpu().native()->CreateUnorderedAccessView(nullptr,nullptr,&uav,views.at(CBCount+TexCount+i));
        std::array<D3D12_SAMPLER_DESC,TexCount> sampler_descs;
        for(auto& desc:sampler_descs)desc=sampler(nullptr);
        for(const auto& r:code.resources) {
            if(r.kind==ShaderResource::Kind::Constants) {
                if(r.slot>=CBCount)throw std::runtime_error("Too many shader constant buffers");
                auto bytes=constants(program,r);if(bytes.empty())bytes.resize(256);
                auto up=gpu().upload(bytes.size());memcpy(up.cpu,bytes.data(),bytes.size());
                D3D12_CONSTANT_BUFFER_VIEW_DESC cb{up.gpu,UINT(bytes.size())};gpu().native()->CreateConstantBufferView(&cb,views.at(r.slot));
            } else {
                auto slot=uniform_slot(program,r.name);
                GLuint id=r.kind==ShaderResource::Kind::Texture?s.texture_slots.at(slot):s.image_slots.at(slot);
                if(!id)continue;auto& t=s.textures.at(id);auto& resource=*t.resource;
                if(r.kind==ShaderResource::Kind::Texture) {
                    srv.Format=resource.desc.Format;srv.ViewDimension=resource.desc.DepthOrArraySize>1?D3D12_SRV_DIMENSION_TEXTURE2DARRAY:D3D12_SRV_DIMENSION_TEXTURE2D;
                    if(resource.desc.DepthOrArraySize>1){srv.Texture2DArray={0,resource.desc.MipLevels,0,resource.desc.DepthOrArraySize,0,0};}
                    else {srv.Texture2D={0,resource.desc.MipLevels,0,0};}
                    gpu().native()->CreateShaderResourceView(resource.object.Get(),&srv,views.at(CBCount+r.slot));sampler_descs.at(r.slot)=sampler(&t);
                } else {
                    uav.Format=resource.desc.Format;gpu().native()->CreateUnorderedAccessView(resource.object.Get(),nullptr,&uav,views.at(CBCount+TexCount+r.slot));
                }
            }
        }
        std::string sampler_key(reinterpret_cast<const char*>(sampler_descs.data()),sizeof(sampler_descs));
        auto sit=s.sampler_tables.find(sampler_key);
        if(sit==s.sampler_tables.end()) {
            auto table=gpu().descriptors(TexCount,true);
            for(uint32_t i=0;i<TexCount;++i)gpu().native()->CreateSampler(&sampler_descs[i],table.at(i));
            sit=s.sampler_tables.emplace(std::move(sampler_key),table).first;
        }
        found=s.binding_tables.emplace(std::move(key),std::make_pair(views,sit->second)).first;
    }
    auto* cmd=gpu().commands();
    if(compute){cmd->SetComputeRootDescriptorTable(root_index,found->second.first.gpu);cmd->SetComputeRootDescriptorTable(root_index+1,found->second.second.gpu);}
    else {cmd->SetGraphicsRootDescriptorTable(root_index,found->second.first.gpu);cmd->SetGraphicsRootDescriptorTable(root_index+1,found->second.second.gpu);}
}
Upload upload_buffer(BufferState& b,uint64_t size) {
    if(size>b.bytes.size())throw std::runtime_error("Draw exceeds vertex/index buffer");
    size=std::max(size,std::min(b.prefetch_bytes,uint64_t(b.bytes.size())));
    if(b.upload_frame!=gpu().frame_serial()||b.upload_version!=b.version||b.upload_size<size) {
        diag::Scope measured(diag::Metric::Upload);diag::count(diag::Count::BufferBytes,size);
        b.uploaded=gpu().upload(size,16);memcpy(b.uploaded.cpu,b.bytes.data(),size);
        b.upload_frame=gpu().frame_serial();b.upload_version=b.version;b.upload_size=size;
    }
    return b.uploaded;
}
void draw_indexed(GLsizei count,GLenum type,const void* indices,GLint basevertex) {
    if(count<=0)return;auto& s=state();auto& p=s.programs.at(s.program);
    diag::count(diag::Count::Draws);diag::count(diag::Count::Indices,uint64_t(count));
    const ShaderCode* vs=nullptr;const ShaderCode* ps=nullptr;
    for(const auto& shader:p.shaders) {
        if(!shader.valid)throw std::runtime_error("Cannot draw with an invalid shader");
        if(shader.stage==Stage::Vertex)vs=s.framebuffer?&shader.flipped:&shader.normal;
        if(shader.stage==Stage::Pixel)ps=&shader.normal;
    }
    if(!vs||!ps)throw std::runtime_error("Missing vertex/pixel shader");
    auto draws=s.framebuffer?s.framebuffers.at(s.framebuffer).draws:std::vector<uint32_t>{0};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE,8> handles{};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};desc.pRootSignature=root_signature(false);
    desc.VS={vs->bytecode.data(),vs->bytecode.size()};desc.PS={ps->bytecode.data(),ps->bytecode.size()};
    desc.NumRenderTargets=UINT(draws.size());desc.SampleDesc.Count=1;desc.SampleMask=UINT_MAX;
    desc.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;desc.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;desc.RasterizerState.DepthClipEnable=TRUE;
    desc.DepthStencilState.DepthEnable=FALSE;desc.DepthStencilState.StencilEnable=FALSE;
    desc.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;desc.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    desc.BlendState.IndependentBlendEnable=TRUE;
    for(size_t i=0;i<draws.size();++i) {
        auto& r=render_target(draws[i]);desc.RTVFormats[i]=r.desc.Format;handles[i]=gpu().rtv(r);
        auto& b=desc.BlendState.RenderTarget[i];const auto& blend=s.blend.at(i);
        b.BlendEnable=s.blend_enabled;b.LogicOpEnable=FALSE;b.SrcBlend=blend_factor(blend.src_color);b.DestBlend=blend_factor(blend.dst_color);b.BlendOp=D3D12_BLEND_OP_ADD;
        b.SrcBlendAlpha=blend_factor(blend.src_alpha==GL_SRC_COLOR?GL_SRC_ALPHA:blend.src_alpha);
        b.DestBlendAlpha=blend_factor(blend.dst_alpha==GL_SRC_COLOR?GL_SRC_ALPHA:blend.dst_alpha);
        b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.LogicOp=D3D12_LOGIC_OP_NOOP;b.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
    uint32_t stride=0;
    for(uint32_t i=0;i<s.attributes.size();++i)if(s.attributes[i].enabled) {
        const auto& a=s.attributes[i];if(!a.components)continue;
        layout.push_back({"TEXCOORD",i,attribute_format(a),0,a.offset,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0});stride=a.stride;
    }
    desc.InputLayout={layout.data(),UINT(layout.size())};
    std::string key;key_append(key,s.framebuffer==0);key_append(key,desc.BlendState);key_append(key,desc.NumRenderTargets);
    key.append(reinterpret_cast<const char*>(desc.RTVFormats),sizeof(desc.RTVFormats));
    for(auto& e:layout){key_append(key,e.SemanticIndex);key_append(key,e.Format);key_append(key,e.AlignedByteOffset);}
    auto it=p.pipelines.find(key);
    if(it==p.pipelines.end()){diag::Scope measured(diag::Metric::Pipeline);diag::count(diag::Count::Pipelines);ComPtr<ID3D12PipelineState> pipeline;check(gpu().native()->CreateGraphicsPipelineState(&desc,IID_PPV_ARGS(&pipeline)),"Create graphics pipeline");it=p.pipelines.emplace(key,pipeline).first;}
    auto& ib=s.buffers.at(s.bound_buffers.at(GL_ELEMENT_ARRAY_BUFFER));auto& vb=s.buffers.at(s.bound_buffers.at(GL_ARRAY_BUFFER));
    const uint32_t index_size=type==GL_UNSIGNED_SHORT?2:type==GL_UNSIGNED_INT?4:0;
    if(!index_size)throw std::runtime_error("Unsupported index type");
    const uint64_t offset=uintptr_t(indices),index_bytes=offset+uint64_t(count)*index_size;
    if(index_bytes>ib.bytes.size())throw std::runtime_error("Index buffer read out of bounds");
    uint32_t maximum=0;
    {
    diag::Scope measured(diag::Metric::IndexScan);
    for(int i=0;i<count;++i) {
        uint32_t value=0;memcpy(&value,ib.bytes.data()+offset+uint64_t(i)*index_size,index_size);maximum=std::max(maximum,value);
    }
    }
    if(basevertex<0)throw std::runtime_error("Negative base vertex is unsupported");
    const auto vsize=(uint64_t(maximum)+basevertex+1)*stride;
    auto vu=upload_buffer(vb,vsize),iu=upload_buffer(ib,ib.bytes.size());
    prepare_textures(*vs,p);prepare_textures(*ps,p);
    auto* cmd=gpu().commands();cmd->SetPipelineState(it->second.Get());cmd->SetGraphicsRootSignature(desc.pRootSignature);
    gpu().bind_descriptor_heaps();bind_resources(*vs,p,0,false);bind_resources(*ps,p,2,false);
    // Targets may also have been bound as textures in a previous pass.
    for(auto index:draws)gpu().transition(render_target(index),D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->OMSetRenderTargets(UINT(draws.size()),handles.data(),FALSE,nullptr);
    const auto target_h=render_target(draws.front()).desc.Height;
    D3D12_VIEWPORT viewport{float(s.viewport_x),float(s.framebuffer?s.viewport_y:int(target_h)-s.viewport_y-s.viewport_height),float(s.viewport_width),float(s.viewport_height),0,1};
    D3D12_RECT scissor{LONG(viewport.TopLeftX),LONG(viewport.TopLeftY),LONG(viewport.TopLeftX+viewport.Width),LONG(viewport.TopLeftY+viewport.Height)};
    cmd->RSSetViewports(1,&viewport);cmd->RSSetScissorRects(1,&scissor);
    D3D12_VERTEX_BUFFER_VIEW vview{vu.gpu,UINT(vsize),stride};D3D12_INDEX_BUFFER_VIEW iview{iu.gpu,UINT(ib.bytes.size()),index_size==2?DXGI_FORMAT_R16_UINT:DXGI_FORMAT_R32_UINT};
    cmd->IASetVertexBuffers(0,1,&vview);cmd->IASetIndexBuffer(&iview);cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawIndexedInstanced(count,1,UINT(offset/index_size),basevertex,0);
}
void dispatch_compute(uint32_t x,uint32_t y,uint32_t z) {
    auto& s=state();auto& p=s.programs.at(s.program);const ShaderCode* code=nullptr;
    for(const auto& shader:p.shaders)if(shader.stage==Stage::Compute&&shader.valid)code=&shader.normal;
    if(!code)throw std::runtime_error("Missing compute shader");
    auto it=p.pipelines.find("compute");
    if(it==p.pipelines.end()){diag::Scope measured(diag::Metric::Pipeline);diag::count(diag::Count::Pipelines);D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};desc.pRootSignature=root_signature(true);desc.CS={code->bytecode.data(),code->bytecode.size()};
        ComPtr<ID3D12PipelineState> pipeline;check(gpu().native()->CreateComputePipelineState(&desc,IID_PPV_ARGS(&pipeline)),"Create compute pipeline");it=p.pipelines.emplace("compute",pipeline).first;}
    prepare_textures(*code,p);
    auto* cmd=gpu().commands();cmd->SetComputeRootSignature(root_signature(true));cmd->SetPipelineState(it->second.Get());gpu().bind_descriptor_heaps();
    bind_resources(*code,p,0,true);cmd->Dispatch(x,y,z);
}
}
