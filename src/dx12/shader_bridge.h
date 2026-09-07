#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
namespace mxl::dx12 {
enum class Stage { Vertex, Pixel, Compute };
struct UniformMember { std::string name; uint32_t offset=0, size=0; };
struct ShaderResource {
    enum class Kind { Constants, Texture, Image };
    Kind kind;
    std::string name;
    uint32_t slot=0, byte_size=0;
    bool globals=false;
    std::vector<UniformMember> members;
};
struct ShaderCode {
    std::string hlsl;
    std::vector<uint8_t> bytecode;
    std::vector<ShaderResource> resources;
};
ShaderCode compile_shader(const std::string& source, Stage stage, bool flip_y, const std::string& name);
std::string convert_slang(const std::string& source, Stage stage,
                          std::vector<std::string>& samplers, std::map<std::string,std::string>& uniforms);
}
