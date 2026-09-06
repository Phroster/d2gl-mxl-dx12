#include "gl_api.h"
#include <iostream>
#include <stdexcept>
#include <fstream>

static GLuint shader(GLenum type,const char* code) {
    GLuint id=glCreateShader(type);glShaderSource(id,1,&code,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if(!ok){char text[8192]{};glGetShaderInfoLog(id,sizeof(text),nullptr,text);throw std::runtime_error(text);}return id;
}
int main() {
    try {
        mxl::dx12::initialize(nullptr);
        GLuint output,input,fbo,vb,ib;
        glGenTextures(1,&output);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,output);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,output,0);
        GLenum attachment=GL_COLOR_ATTACHMENT0;glDrawBuffers(1,&attachment);
        glViewport(0,0,4,4);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        glGenTextures(1,&input);glActiveTexture(GL_TEXTURE0+1);glBindTexture(GL_TEXTURE_2D,input);
        const uint8_t pixels[]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        const float vertices[]={-1,-1,0,0, 1,-1,1,0, 1,1,1,1, -1,1,0,1};
        const uint32_t indices[]={0,1,2,2,3,0};
        glGenBuffers(1,&vb);glBindBuffer(GL_ARRAY_BUFFER,vb);glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
        glGenBuffers(1,&ib);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ib);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,16,nullptr);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,16,(void*)8);
        GLuint p=glCreateProgram();
        glAttachShader(p,shader(GL_VERTEX_SHADER,R"(#version 450
layout(location=0) in vec2 Position;
layout(location=1) in vec2 TexCoord;
uniform mat4 u_MVP;
out vec2 v_TexCoord;
void main(){gl_Position=u_MVP*vec4(Position,0,1);v_TexCoord=TexCoord;})"));
        glAttachShader(p,shader(GL_FRAGMENT_SHADER,R"(#version 450
in vec2 v_TexCoord;
uniform sampler2D u_Texture;
uniform float lod;
layout(location=0) out vec4 Color;
void main(){Color=textureLod(u_Texture,v_TexCoord,lod);})"));
        glLinkProgram(p);glUseProgram(p);
        const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        glUniformMatrix4fv(glGetUniformLocation(p,"u_MVP"),1,GL_FALSE,identity);
        glUniform1i(glGetUniformLocation(p,"u_Texture"),1);glUniform1f(glGetUniformLocation(p,"lod"),0);glDisable(GL_BLEND);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
        uint8_t result[64]{};glReadPixels(0,0,4,4,GL_RGBA,GL_UNSIGNED_BYTE,result);
        for(int y=0;y<4;++y)for(int x=0;x<4;++x)for(int c=0;c<4;++c)
            if(result[(y*4+x)*4+c]!=pixels[((y/2)*2+x/2)*4+c]) {
                std::cerr<<"Mismatch "<<x<<","<<y<<","<<c<<" = "<<int(result[(y*4+x)*4+c])<<"\n";return 3;
            }
        std::cout<<"PASS: native DX12 textured quad, GLSL-to-HLSL, matrix uniform, texture sampler, orientation and GPU readback.\n";
        glGenerateMipmap(GL_TEXTURE_2D);
        glUniform1f(glGetUniformLocation(p,"lod"),1);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
        glReadPixels(0,0,4,4,GL_RGBA,GL_UNSIGNED_BYTE,result);
        for(int i=0;i<16;++i)for(int c=0;c<4;++c) {
            int expected=c==3?255:128;
            if(std::abs(int(result[i*4+c])-expected)>1) {
                std::cerr<<"Mipmap mismatch: "<<i<<","<<c<<" = "<<int(result[i*4+c])<<"\n";return 4;
            }
        }
        std::cout<<"PASS: GPU-generated mip chain sampled with explicit mip level.\n";
        mxl::dx12::shutdown();
    } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
