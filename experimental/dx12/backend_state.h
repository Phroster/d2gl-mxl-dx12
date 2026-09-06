#pragma once
#define MXL_DX12_API_IMPLEMENTATION
#include "gl_api.h"
#include "shader_bridge.h"
#include <unordered_map>
#include <map>
#include <array>

namespace mxl::dx12 {
struct BufferState {
    std::vector<uint8_t> bytes;
    uint64_t version=0,upload_version=UINT64_MAX,upload_frame=UINT64_MAX;
    uint64_t upload_size=0;
    Upload uploaded{};
};
struct TextureState {
    std::shared_ptr<Resource> resource;
    GLenum target=GL_TEXTURE_2D;
    GLint filter_min=GL_NEAREST,filter_mag=GL_NEAREST,wrap_s=GL_CLAMP_TO_EDGE,wrap_t=GL_CLAMP_TO_EDGE;
    bool mip_requested=false,mips_dirty=false;
};
struct FramebufferState {
    std::array<GLuint,8> attachments{};
    std::vector<uint32_t> draws{0};
};
struct Attribute {
    bool enabled=false,integer=false,normalized=false;
    GLenum type=GL_FLOAT;uint32_t components=0,stride=0,offset=0;
};
struct Blend {
    GLenum src_color=GL_SRC_ALPHA,dst_color=GL_ONE_MINUS_SRC_ALPHA,src_alpha=GL_ONE,dst_alpha=GL_ONE_MINUS_SRC_ALPHA;
};
struct ShaderState {
    Stage stage=Stage::Vertex;
    std::string source,error;
    ShaderCode normal,flipped;
    bool valid=false;
};
struct ProgramState {
    uint64_t version=0;
    std::vector<ShaderState> shaders;
    std::vector<std::string> locations,blocks;
    std::map<std::string,std::vector<uint8_t>> values;
    std::map<std::string,uint32_t> block_bindings;
    std::map<std::string,ComPtr<ID3D12PipelineState>> pipelines;
};
struct State {
    std::unique_ptr<Device> device;
    GLuint next_id=1,program=0,framebuffer=0,texture_unit=0;
    std::unordered_map<GLuint,BufferState> buffers;
    std::unordered_map<GLuint,TextureState> textures;
    std::unordered_map<GLuint,FramebufferState> framebuffers;
    std::unordered_map<GLuint,ShaderState> shaders;
    std::unordered_map<GLuint,ProgramState> programs;
    std::unordered_map<GLenum,GLuint> bound_buffers;
    struct Ubo {GLuint buffer=0;uint32_t offset=0,size=0;};
    std::unordered_map<GLuint,Ubo> ubos;
    std::array<GLuint,32> texture_slots{};
    std::array<GLuint,16> image_slots{};
    std::array<Attribute,16> attributes{};
    std::array<Blend,8> blend{};
    bool blend_enabled=true;
    std::array<float,4> clear_color{};
    int viewport_x=0,viewport_y=0,viewport_width=1,viewport_height=1;
    uint32_t read_attachment=0;
    ComPtr<ID3D12RootSignature> graphics_root,compute_root;
    uint64_t sampler_frame=UINT64_MAX;
    std::unordered_map<std::string,DescriptorTable> sampler_tables;
    std::unordered_map<std::string,std::pair<DescriptorTable,DescriptorTable>> binding_tables;
    std::shared_ptr<Resource> dummy_texture;
};
State& state();
Resource& render_target(uint32_t index);
TextureState& bound_texture(GLenum target);
void set_uniform(GLint location,const void* data,size_t bytes);
void draw_indexed(GLsizei count,GLenum type,const void* indices,GLint basevertex);
void dispatch_compute(uint32_t x,uint32_t y,uint32_t z);
void generate_mips(TextureState& texture);
}
