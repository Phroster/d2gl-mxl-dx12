#include "../../d2gl/d2gl/vendor/include/glslang/glslang.h"
#include "shader_bridge.h"
#include <map>
#include <exception>
namespace glslang {
Result getGLSLCode(ShaderStage stage,GLVersion,const std::string& source,const std::string&) {
    Result result;
    try {
        std::map<std::string,std::string> uniforms;
        result.source=mxl::dx12::convert_slang(source,stage==ShaderStage::Vertex?mxl::dx12::Stage::Vertex:mxl::dx12::Stage::Pixel,result.samplers,uniforms);
        result.uniforms.insert(uniforms.begin(),uniforms.end());
        result.result=true;
    } catch(const std::exception& e) {result.err_msg=e.what();}
    return result;
}
}
