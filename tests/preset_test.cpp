#include "shader_bridge.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
int main(int argc,char** argv){
 if(argc!=2)return 2;size_t passed=0,failed=0;
 for(const auto& entry:std::filesystem::directory_iterator(argv[1])){
    auto ext=entry.path().extension().string();if(ext!=".vert"&&ext!=".frag")continue;
    try{
        std::ifstream input(entry.path());std::ostringstream source;source<<input.rdbuf();
        auto stage=ext==".vert"?mxl::dx12::Stage::Vertex:mxl::dx12::Stage::Pixel;
        std::vector<std::string> samplers;std::map<std::string,std::string> uniforms;
        auto glsl=mxl::dx12::convert_slang(source.str(),stage,samplers,uniforms);
        auto compiled=mxl::dx12::compile_shader(glsl,stage,false,entry.path().filename().string());
        if(compiled.bytecode.empty())throw std::runtime_error("Empty shader");
        ++passed;
    }catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<entry.path().filename().string()<<"\n"<<e.what()<<"\n";}
 }
 std::cout<<"Preset shader stages: "<<passed<<" passed, "<<failed<<" failed.\n";
 return failed?1:0;
}
