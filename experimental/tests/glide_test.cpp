#include "gl_api.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cmath>
struct V {uint16_t xy[2];float uv[2];uint32_t color1,color2;uint16_t tex[2];uint8_t flags[4];};
static GLuint compile(GLenum type,const std::string& text) {
    auto id=glCreateShader(type);const char* ptr=text.c_str();glShaderSource(id,1,&ptr,nullptr);glCompileShader(id);GLint ok;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if(!ok){char e[8192];glGetShaderInfoLog(id,sizeof(e),nullptr,e);throw std::runtime_error(e);}return id;
}
int main(){
 try{
    mxl::dx12::initialize(nullptr);
    GLuint targets[3],fbo;glGenTextures(3,targets);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    GLenum draws[3];
    for(int i=0;i<3;++i){
        glActiveTexture(GL_TEXTURE0+8+i);glBindTexture(GL_TEXTURE_2D,targets[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        draws[i]=GL_COLOR_ATTACHMENT0+i;glFramebufferTexture2D(GL_FRAMEBUFFER,draws[i],GL_TEXTURE_2D,targets[i],0);
    }
    glDrawBuffers(3,draws);glViewport(0,0,4,4);glDisable(GL_BLEND);
    GLuint tex;glGenTextures(1,&tex);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D_ARRAY,tex);
    uint8_t pixels[8]={1,1,1,1,1,1,1,1};glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_R8,2,2,2,0,GL_RED,GL_UNSIGNED_BYTE,pixels);
    V vertices[4]={{{0xbc00,0xbc00},{0,0},0xffffffff,0,{0,0},{0,0,0,0}},
        {{0x3c00,0xbc00},{1,0},0xffffffff,0,{0,0},{0,0,0,0}},
        {{0x3c00,0x3c00},{1,1},0xffffffff,0,{0,0},{0,0,0,0}},
        {{0xbc00,0x3c00},{0,1},0xffffffff,0,{0,0},{0,0,0,0}}};
    GLuint vb,ib,ubo;glGenBuffers(1,&vb);glBindBuffer(GL_ARRAY_BUFFER,vb);glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    uint32_t indices[]={0,1,2,2,3,0};glGenBuffers(1,&ib);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ib);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    for(int i=0;i<6;++i)glEnableVertexAttribArray(i);
    glVertexAttribPointer(0,2,GL_HALF_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,xy));
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,uv));
    glVertexAttribPointer(2,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(V),(void*)offsetof(V,color1));
    glVertexAttribPointer(3,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(V),(void*)offsetof(V,color2));
    glVertexAttribIPointer(4,2,GL_UNSIGNED_SHORT,sizeof(V),(void*)offsetof(V,tex));
    glVertexAttribIPointer(5,4,GL_UNSIGNED_BYTE,sizeof(V),(void*)offsetof(V,flags));
    float colors[512][4]{};colors[1][0]=1;colors[1][3]=1;
    for(int i=0;i<256;++i)for(int c=0;c<4;++c)colors[256+i][c]=i/255.0f;
    glGenBuffers(1,&ubo);glBindBuffer(GL_UNIFORM_BUFFER,ubo);glBufferData(GL_UNIFORM_BUFFER,sizeof(colors),colors,GL_STATIC_DRAW);
    glBindBufferRange(GL_UNIFORM_BUFFER,7,ubo,0,sizeof(colors));
    std::ifstream input(std::filesystem::path(MXL_SOURCE_DIR)/"d2gl/d2gl/src/graphic/shaders/glide.glsl");std::stringstream source;source<<input.rdbuf();
    auto program=glCreateProgram();glAttachShader(program,compile(GL_VERTEX_SHADER,"#version 450\n#define VERTEX 1\n"+source.str()));
    glAttachShader(program,compile(GL_FRAGMENT_SHADER,"#version 450\n#define FRAGMENT 1\n"+source.str()));glLinkProgram(program);glUseProgram(program);
    float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    glUniformMatrix4fv(glGetUniformLocation(program,"u_MVP"),1,GL_FALSE,identity);
    glUniform1i(glGetUniformLocation(program,"u_Texture"),0);glUniformBlockBinding(program,glGetUniformBlockIndex(program,"ubo_Colors"),7);
    for(int pass=0;pass<2;++pass){
        for(auto& v:vertices)v.flags[3]=pass?2:0;
        glBindBuffer(GL_ARRAY_BUFFER,vb);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(vertices),vertices);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
        for(int rt=0;rt<3;++rt){
            glReadBuffer(GL_COLOR_ATTACHMENT0+rt);uint8_t data[64]{};glReadPixels(0,0,4,4,GL_RGBA,GL_UNSIGNED_BYTE,data);
            for(int i=0;i<16;++i)for(int c=0;c<4;++c){
                int expected=(!pass&&rt==0)?((c==0||c==3)?255:0):(pass&&rt==1)?(c==0?255:c==3?230:0):0;
                if(std::abs(int(data[i*4+c])-expected)>1){std::cerr<<"Mismatch pass="<<pass<<" target="<<rt<<" c="<<c<<" actual="<<int(data[i*4+c])<<" expected="<<expected<<"\n";return 2;}
            }
        }
    }
    std::cout<<"PASS: actual Glide shader, packed half/integer vertices, R8 texture array, palette/gamma UBO and all three render targets.\n";
    mxl::dx12::shutdown();
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
