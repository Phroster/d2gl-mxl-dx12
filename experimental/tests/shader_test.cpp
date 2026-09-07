#include "shader_bridge.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
int main() {
    try {
        size_t passed=0;
        auto root=std::filesystem::path(MXL_SOURCE_DIR)/"d2gl/d2gl/src/graphic/shaders";
        for(const auto& file:std::filesystem::directory_iterator(root)) {
            if(file.path().extension()!=".glsl") continue;
            std::ifstream input(file.path()); std::stringstream s; s<<input.rdbuf();
            for(auto stage:{mxl::dx12::Stage::Vertex,mxl::dx12::Stage::Pixel}) {
                auto text=std::string("#version 450\n#define ")+(stage==mxl::dx12::Stage::Vertex?"VERTEX":"FRAGMENT")+" 1\n"+s.str();
                auto code=mxl::dx12::compile_shader(text,stage,false,file.path().filename().string());
                if(code.bytecode.empty()) return 2;
                std::cout<<file.path().filename().string()<<" "<<(stage==mxl::dx12::Stage::Vertex?"VS":"PS")<<" PASS "<<code.bytecode.size()<<" bytes\n";
                ++passed;
            }
            if(s.str().find("#elif COMPUTE")!=std::string::npos) {
                auto code=mxl::dx12::compile_shader("#version 450\n#define COMPUTE 1\n"+s.str(),mxl::dx12::Stage::Compute,false,file.path().filename().string());
                if(code.bytecode.empty())return 3;
                std::cout<<file.path().filename().string()<<" CS PASS "<<code.bytecode.size()<<" bytes\n";++passed;
            }
        }
        std::cout<<"PASS: "<<passed<<" built-in D2GL shader stages translated and compiled for D3D12.\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
