#include "shader_bridge.h"
#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_hlsl.hpp>
#include <spirv_glsl.hpp>
#include <mutex>
#include <stdexcept>
#include <regex>
#include <algorithm>
#include <functional>

namespace mxl::dx12 {
using Microsoft::WRL::ComPtr;
static std::vector<uint32_t> to_spirv(const std::string& text, Stage stage, bool relaxed) {
    static std::once_flag init;
    std::call_once(init, [] { if(!glslang::InitializeProcess()) throw std::runtime_error("glslang initialization failed"); });
    const auto lang=stage==Stage::Vertex ? EShLangVertex : stage==Stage::Pixel ? EShLangFragment : EShLangCompute;
    glslang::TShader shader(lang);
    std::string source=std::regex_replace(text,std::regex("#version[^\n]*"),"#version 450");
    // The bundled legacy EGA preset has a duplicated selector and an undefined
    // fallthrough. Define those paths before cross-compilation; valid selectors
    // retain the original filter, and an invalid selector passes the colour through.
    if(source.find("vec3 nearest_rgbi")!=std::string::npos) {
        const std::string duplicate="else if (palette == 1.0) set = 2;";
        if(auto pos=source.find(duplicate);pos!=std::string::npos)
            source.replace(pos,duplicate.size(),"else if (palette == 0.0) set = 2;");
        const std::string tail="else if (palette == 0.0) return simcga_palette[idx];";
        if(auto pos=source.find(tail);pos!=std::string::npos)
            source.replace(pos,tail.size(),tail+"\n  return original;");
    }
    const char* strings[]={source.c_str()};
    shader.setStrings(strings,1);
    shader.setEnvInput(glslang::EShSourceGlsl,lang,glslang::EShClientVulkan,450);
    shader.setEnvClient(glslang::EShClientVulkan,glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv,glslang::EShTargetSpv_1_0);
    shader.setAutoMapBindings(true);
    shader.setAutoMapLocations(true);
    if(relaxed) {
        shader.setEnvInputVulkanRulesRelaxed();
        shader.setGlobalUniformBlockName("MXLGlobals");
        shader.setGlobalUniformSet(0);
        shader.setGlobalUniformBinding(0);
    }
    auto messages=EShMessages(EShMsgSpvRules|EShMsgVulkanRules);
    if(!shader.parse(GetDefaultResources(),450,false,messages))
        throw std::runtime_error(std::string(shader.getInfoLog())+shader.getInfoDebugLog());
    glslang::TProgram program;
    program.addShader(&shader);
    if(!program.link(messages) || !program.mapIO())
        throw std::runtime_error(std::string(program.getInfoLog())+program.getInfoDebugLog());
    std::vector<uint32_t> code;
    glslang::SpvOptions options; options.disableOptimizer=true;
    glslang::GlslangToSpv(*program.getIntermediate(lang),code,&options);
    return code;
}
ShaderCode compile_shader(const std::string& source, Stage stage, bool flip_y, const std::string& name) {
    spirv_cross::CompilerHLSL compiler(to_spirv(source,stage,true));
    auto common=compiler.get_common_options();
    common.vertex.flip_vert_y=flip_y; common.vertex.fixup_clipspace=true;
    common.force_zero_initialized_variables=true;
    compiler.set_common_options(common);
    auto hlsl=compiler.get_hlsl_options(); hlsl.shader_model=50;
    compiler.set_hlsl_options(hlsl);
    ShaderCode output;
    auto resources=compiler.get_shader_resources();
    uint32_t cb_slot=0,tex_slot=0,image_slot=0;
    for(const auto& r:resources.uniform_buffers) {
        ShaderResource item{ShaderResource::Kind::Constants,r.name,cb_slot++};
        const auto& type=compiler.get_type(r.base_type_id);
        item.byte_size=uint32_t(compiler.get_declared_struct_size(type));
        item.globals=r.name=="MXLGlobals";
        std::function<void(uint32_t,const std::string&,uint32_t)> members;
        members=[&](uint32_t type_id,const std::string& prefix,uint32_t offset) {
            const auto& structure=compiler.get_type(type_id);
            for(uint32_t i=0;i<structure.member_types.size();++i) {
                const auto name=prefix+compiler.get_member_name(type_id,i);
                const auto member_offset=offset+compiler.type_struct_member_offset(structure,i);
                const auto& member_type=compiler.get_type(structure.member_types[i]);
                if(member_type.basetype==spirv_cross::SPIRType::Struct&&member_type.array.empty())
                    members(structure.member_types[i],name+".",member_offset);
                else item.members.push_back({name,member_offset,uint32_t(compiler.get_declared_struct_member_size(structure,i))});
            }
        };
        members(r.base_type_id,"",0);
        compiler.set_decoration(r.id,spv::DecorationDescriptorSet,0);
        compiler.set_decoration(r.id,spv::DecorationBinding,item.slot);
        output.resources.push_back(item);
    }
    for(const auto& r:resources.sampled_images) {
        ShaderResource item{ShaderResource::Kind::Texture,r.name,tex_slot++};
        compiler.set_decoration(r.id,spv::DecorationDescriptorSet,0);
        compiler.set_decoration(r.id,spv::DecorationBinding,item.slot);
        output.resources.push_back(item);
    }
    for(const auto& r:resources.storage_images) {
        ShaderResource item{ShaderResource::Kind::Image,r.name,image_slot++};
        compiler.set_decoration(r.id,spv::DecorationDescriptorSet,0);
        compiler.set_decoration(r.id,spv::DecorationBinding,item.slot);
        output.resources.push_back(item);
    }
    output.hlsl=compiler.compile();
    ComPtr<ID3DBlob> compiled,errors;
    const char* profile=stage==Stage::Vertex ? "vs_5_0" : stage==Stage::Pixel ? "ps_5_0" : "cs_5_0";
    HRESULT hr=D3DCompile(output.hlsl.data(),output.hlsl.size(),name.c_str(),nullptr,nullptr,
        "main",profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&compiled,&errors);
    if(FAILED(hr)) throw std::runtime_error("HLSL "+name+": "+
        (errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()) : "compile failed"));
    const auto* begin=static_cast<const uint8_t*>(compiled->GetBufferPointer());
    output.bytecode.assign(begin,begin+compiled->GetBufferSize());
    return output;
}
std::string convert_slang(const std::string& source, Stage stage,
                          std::vector<std::string>& samplers, std::map<std::string,std::string>& uniforms) {
    spirv_cross::CompilerGLSL compiler(to_spirv(source,stage,false));
    auto options=compiler.get_common_options();
    options.version=450; options.es=false; options.vulkan_semantics=false;
    options.emit_uniform_buffer_as_plain_uniforms=true;
    compiler.set_common_options(options);
    auto resources=compiler.get_shader_resources();
    for(const auto& r:resources.sampled_images) samplers.push_back(r.name);
    for(const auto& r:resources.uniform_buffers) {
        const auto& type=compiler.get_type(r.base_type_id);
        auto instance=compiler.get_name(r.id);
        if(instance.empty()) { instance="mxl_"+std::to_string(r.id); compiler.set_name(r.id,instance); }
        for(uint32_t i=0;i<type.member_types.size();++i)
            uniforms[compiler.get_member_name(r.base_type_id,i)]=instance;
    }
    for(const auto& r:resources.push_constant_buffers) {
        const auto& type=compiler.get_type(r.base_type_id);
        auto instance=compiler.get_name(r.id);
        if(instance.empty()) { instance="mxl_"+std::to_string(r.id); compiler.set_name(r.id,instance); }
        for(uint32_t i=0;i<type.member_types.size();++i)
            uniforms[compiler.get_member_name(r.base_type_id,i)]=instance;
    }
    return compiler.compile();
}
}
