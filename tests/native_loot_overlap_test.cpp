// SPDX-License-Identifier: GPL-3.0-or-later
#include "gl_api.h"
#include "native_loot_cells.h"
#include "native_loot_render.h"
#include <glm/detail/type_half.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
constexpr unsigned width=256, height=192, extent=128;
struct Vertex { uint16_t xy[2]; float uv[2]; uint32_t colour1,colour2; uint16_t tex[2]; uint8_t flags[4]; };
void require(bool ok,const char* text) { if(!ok) throw std::runtime_error(text); }
uint32_t word(const std::vector<uint8_t>& data,size_t at) { uint32_t value;std::memcpy(&value,data.data()+at,4);return value; }
GLuint compile(GLenum type,const std::string& source) {
    auto shader=glCreateShader(type);auto* ptr=source.c_str();glShaderSource(shader,1,&ptr,nullptr);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);require(ok,"shader compile failed");return shader;
}
}
int main() {
    try {
        using namespace mxl::native_loot;
        // Decode the actual animated potion art, with transparent texels and
        // the game's indexed palette, then use the real DX12 Glide shader.
        std::vector<uint8_t> texels(extent*extent*48);
        const auto size=effect_size(1,Style::Orb);
        require(size.width<=extent && size.height<=extent,"potion fixture too large");
        for(unsigned colour=0;colour<2;++colour) {
            const auto dc6=make_cells(1,colour?0:4,false,Style::Orb);
            for(unsigned frame=0;frame<24;++frame) {
                auto cell=word(dc6,24+frame*4);auto at=cell+32;unsigned x=0,row=0;
                while(row<unsigned(size.height)) {
                    const auto run=dc6.at(at++);
                    if(run==128) {x=0;++row;continue;}
                    const auto count=run&127;
                    if(!(run&128)) for(unsigned n=0;n<count;++n)
                        texels[(colour*24+frame)*extent*extent+(size.height-1-row)*extent+x+n]=dc6.at(at++);
                    x+=count;require(x<=unsigned(size.width),"invalid potion run");
                }
            }
        }
        mxl::dx12::initialize(nullptr);
        GLuint target,fbo,texture,vb,ib,ubo;
        glGenTextures(1,&target);glActiveTexture(GL_TEXTURE0+8);glBindTexture(GL_TEXTURE_2D,target);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
        const GLenum output=GL_COLOR_ATTACHMENT0;glDrawBuffers(1,&output);glReadBuffer(output);glViewport(0,0,width,height);
        glGenTextures(1,&texture);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_R8,extent,extent,48,0,GL_RED,GL_UNSIGNED_BYTE,texels.data());
        glGenBuffers(1,&vb);glBindBuffer(GL_ARRAY_BUFFER,vb);glBufferData(GL_ARRAY_BUFFER,sizeof(Vertex)*4,nullptr,GL_DYNAMIC_DRAW);
        uint32_t indices[]={0,1,2,2,3,0};glGenBuffers(1,&ib);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ib);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
        for(unsigned i=0;i<6;++i) glEnableVertexAttribArray(i);
        glVertexAttribPointer(0,2,GL_HALF_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,xy));
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,uv));
        glVertexAttribPointer(2,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(Vertex),(void*)offsetof(Vertex,colour1));
        glVertexAttribPointer(3,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(Vertex),(void*)offsetof(Vertex,colour2));
        glVertexAttribIPointer(4,2,GL_UNSIGNED_SHORT,sizeof(Vertex),(void*)offsetof(Vertex,tex));
        glVertexAttribIPointer(5,4,GL_UNSIGNED_BYTE,sizeof(Vertex),(void*)offsetof(Vertex,flags));
        float colours[512][4]{};
        for(unsigned i=0;i<256;++i) {
            for(unsigned c=0;c<3;++c) colours[i][c]=unit_palette[i][c]/255.f;
            colours[i][3]=1;
            for(unsigned c=0;c<4;++c) colours[i+256][c]=i/255.f;
        }
        glGenBuffers(1,&ubo);glBindBuffer(GL_UNIFORM_BUFFER,ubo);glBufferData(GL_UNIFORM_BUFFER,sizeof(colours),colours,GL_STATIC_DRAW);glBindBufferRange(GL_UNIFORM_BUFFER,7,ubo,0,sizeof(colours));
        std::ifstream file(std::filesystem::path(MXL_SOURCE_DIR)/"d2gl/d2gl/src/graphic/shaders/glide.glsl");std::stringstream source;source<<file.rdbuf();
        auto program=glCreateProgram();glAttachShader(program,compile(GL_VERTEX_SHADER,"#version 450\n#define VERTEX 1\n"+source.str()));
        glAttachShader(program,compile(GL_FRAGMENT_SHADER,"#version 450\n#define FRAGMENT 1\n"+source.str()));glLinkProgram(program);glUseProgram(program);
        float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};glUniformMatrix4fv(glGetUniformLocation(program,"u_MVP"),1,GL_FALSE,identity);
        glUniform1i(glGetUniformLocation(program,"u_Texture"),0);glUniformBlockBinding(program,glGetUniformBlockIndex(program,"ubo_Colors"),7);
        glEnable(GL_BLEND);
        const bool additive=effect_draw_mode(1)==3;
        glBlendFuncSeparate(GL_ONE,additive?GL_ONE:GL_ZERO,additive?GL_ZERO:GL_ONE,additive?GL_ONE:GL_ZERO);
        auto draw=[&](unsigned item,unsigned frame,unsigned colour,unsigned chromakey) {
            const float left=76.f+item*7,top=54.f+item*3;
            const float xy[4][2]={{left,top},{left+size.width,top},{left+size.width,top+size.height},{left,top+size.height}};
            const float uv[4][2]={{0,0},{size.width/float(extent),0},{size.width/float(extent),size.height/float(extent)},{0,size.height/float(extent)}};
            Vertex vertices[4]{};
            for(unsigned i=0;i<4;++i) {
                vertices[i].xy[0]=glm::detail::toFloat16(xy[i][0]*2/width-1);
                vertices[i].xy[1]=glm::detail::toFloat16(1-xy[i][1]*2/height);
                std::memcpy(vertices[i].uv,uv[i],sizeof(uv[i]));vertices[i].colour1=0xffffffff;
                vertices[i].tex[0]=uint16_t(colour*24+frame);vertices[i].flags[0]=uint8_t(chromakey);
            }
            glBindBuffer(GL_ARRAY_BUFFER,vb);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(vertices),vertices);glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
        };
        unsigned differing=0,comparisons=0;std::vector<uint8_t> first(width*height*4),second(first.size());
        for(unsigned colour:{0u,1u}) for(unsigned chromakey:{0u,1u}) for(unsigned frame=0;frame<24;++frame) {
            auto render=[&](bool reverse,std::vector<uint8_t>& pixels) {
                glClearColor(.03f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);
                for(unsigned j=0;j<2;++j) {const auto item=reverse?1-j:j;draw(item,(frame+item*7)%24,item?colour:0,chromakey);}
                glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            };
            render(false,first);render(true,second);++comparisons;
            for(size_t i=0;i<first.size();++i) if(i%4!=3 && std::abs(int(first[i])-int(second[i]))>1) ++differing;
            unsigned lit=0;for(size_t i=0;i<first.size();i+=4) if(first[i]>40 || first[i+1]>40 || first[i+2]>40) ++lit;
            require(lit>100,"potions did not render");
        }
        mxl::dx12::shutdown();
        std::cout<<"Potion overlap: "<<comparisons<<" frame/order comparisons; differing RGB channels="<<differing<<'\n';
        require(differing==0,"overlapping potion animations overwrite one another when draw order changes");
        std::cout<<"PASS: real potion sprites retain additive overlap across all animation frames.\n";
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
