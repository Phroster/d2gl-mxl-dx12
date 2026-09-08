#include "backend_state.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include "diagnostics.h"
#include "audio_diagnostics.h"
#include "sound_probe.h"
#include "sound_cancel.h"
#include "asset_probe.h"
#include "tile_cache.h"
#include "reveal_probe.h"

namespace mxl::dx12 {
State& state() { static State* value=new State;return *value; }
static void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void initialize(HWND window) {
    require(!state().device,"DX12 already initialized");
    try {if(diag::start(window)){diag::start_audio();diag::start_sound_probe();}}catch(...){diag::note("diagnostics_initialization_failed");diag::stop();}
    // Tile caching, like act-entry reveal, is independent of opt-in recording.
    diag::start_assets();
    mxl::sound_cancel::start();
    // Act-entry reveal is a gameplay feature, independent of opt-in recording.
    diag::start_reveal_probe(window);
    state().device=std::make_unique<Device>(window,false);
}
Device& gpu() {require(bool(state().device),"DX12 is not initialized");return *state().device;}
void shutdown() { tiles::end_frame();if(state().device) {gpu().wait_idle();state()=State{};} }
void present(bool vsync) {gpu().present(vsync);}
void resize(uint32_t w,uint32_t h) {gpu().resize(w,h);}
uint32_t live_validation_errors() {return gpu().validation_errors();}
Resource& render_target(uint32_t index) {
    auto& s=state();if(!s.framebuffer) return gpu().back_buffer();
    auto id=s.framebuffers.at(s.framebuffer).attachments.at(index);
    require(id && s.textures.at(id).resource,"Missing framebuffer attachment");
    return *s.textures.at(id).resource;
}
TextureState& bound_texture(GLenum target) {
    auto& s=state();auto id=s.texture_slots.at(s.texture_unit);require(id!=0,"No texture bound");
    auto& t=s.textures.at(id);t.target=target;return t;
}
void glGenBuffers(GLsizei n,GLuint* ids) {for(int i=0;i<n;++i){ids[i]=state().next_id++;state().buffers.emplace(ids[i],BufferState{});}}
void glBindBuffer(GLenum target,GLuint id) {state().bound_buffers[target]=id;}
void glBufferData(GLenum target,GLsizeiptr size,const void* data,GLenum) {
    require(size>=0,"Negative buffer size");auto& b=state().buffers.at(state().bound_buffers.at(target));
    b.bytes.resize(size);if(data&&size) std::memcpy(b.bytes.data(),data,size);++b.version;
    b.prefetch_bytes=data?uint64_t(size):0;
}
void glBufferSubData(GLenum target,GLintptr offset,GLsizeiptr size,const void* data) {
    auto& b=state().buffers.at(state().bound_buffers.at(target));
    require(offset>=0&&size>=0&&uint64_t(offset)+size<=b.bytes.size(),"Buffer update out of bounds");
    if(size) std::memcpy(b.bytes.data()+offset,data,size);++b.version;
    // D2GL writes the full live frame at offset zero before its draws. Upload
    // that range once, instead of re-copying an ever-growing prefix per draw.
    const auto end=uint64_t(offset)+uint64_t(size);
    b.prefetch_bytes=offset==0?end:std::max(b.prefetch_bytes,end);
}
void glBindBufferRange(GLenum target,GLuint slot,GLuint buffer,GLintptr offset,GLsizeiptr size) {
    require(target==GL_UNIFORM_BUFFER&&offset>=0&&size>=0,"Unsupported buffer range");
    state().ubos[slot]={buffer,uint32_t(offset),uint32_t(size)};
}
void glDeleteBuffers(GLsizei n,const GLuint* ids) {
    for(int i=0;i<n;++i){
        for(auto& [target,bound]:state().bound_buffers)if(bound==ids[i])bound=0;
        for(auto& [slot,ubo]:state().ubos)if(ubo.buffer==ids[i])ubo={};
        state().buffers.erase(ids[i]);
    }
}
void glGenVertexArrays(GLsizei n,GLuint* ids) {for(int i=0;i<n;++i) ids[i]=state().next_id++;}
void glBindVertexArray(GLuint) {}
void glDeleteVertexArrays(GLsizei,const GLuint*) {}
void glEnableVertexAttribArray(GLuint i) {state().attributes.at(i).enabled=true;}
void glDisableVertexAttribArray(GLuint i) {state().attributes.at(i).enabled=false;}
void glVertexAttribPointer(GLuint i,GLint size,GLenum type,GLboolean normalized,GLsizei stride,const void* ptr) {
    auto& a=state().attributes.at(i);a.components=size;a.type=type;a.normalized=normalized!=0;a.integer=false;a.stride=stride;a.offset=uint32_t(uintptr_t(ptr));
}
void glVertexAttribIPointer(GLuint i,GLint size,GLenum type,GLsizei stride,const void* ptr) {
    glVertexAttribPointer(i,size,type,false,stride,ptr);state().attributes.at(i).integer=true;
}
void glGenTextures(GLsizei n,GLuint* ids) {for(int i=0;i<n;++i){ids[i]=state().next_id++;state().textures.emplace(ids[i],TextureState{});}}
void glActiveTexture(GLenum unit) {require(unit>=GL_TEXTURE0&&unit<GL_TEXTURE0+32,"Texture slot out of range");state().texture_unit=unit-GL_TEXTURE0;}
void glBindTexture(GLenum target,GLuint id) {state().texture_slots.at(state().texture_unit)=id;if(id) state().textures.at(id).target=target;}
void glTexParameteri(GLenum target,GLenum pname,GLint value) {
    auto& t=bound_texture(target);
    switch(pname) {case GL_TEXTURE_MIN_FILTER:t.filter_min=value;break;case GL_TEXTURE_MAG_FILTER:t.filter_mag=value;break;
        case GL_TEXTURE_WRAP_S:t.wrap_s=value;break;case GL_TEXTURE_WRAP_T:t.wrap_t=value;break;
        default:throw std::runtime_error("Unsupported DX12 texture parameter");}
}
static DXGI_FORMAT texture_format(GLint format) {
    switch(format) {case GL_R8:return DXGI_FORMAT_R8_UNORM;case GL_RGBA8:case GL_RGB8:return DXGI_FORMAT_R8G8B8A8_UNORM;
        case GL_RGBA16F:return DXGI_FORMAT_R16G16B16A16_FLOAT;case GL_RGBA32F:return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default:throw std::runtime_error("Unsupported DX12 texture format");}
}
static uint32_t channels(GLenum format) {return format==GL_RED?1:format==GL_RGB?3:4;}
void glTexImage3D(GLenum target,GLint level,GLint format,GLsizei w,GLsizei h,GLsizei depth,GLint border,GLenum input_format,GLenum type,const void* pixels) {
    require(level==0&&border==0&&w>0&&h>0&&depth>0,"Unsupported texture allocation");
    auto& t=bound_texture(target);if(t.resource){gpu().keep_alive(t.resource);gpu().release_rtv(*t.resource);}
    auto flags=D3D12_RESOURCE_FLAGS(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET|D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    t.resource=gpu().texture(w,h,depth,texture_format(format),flags);
    if(pixels) glTexSubImage3D(target,0,0,0,0,w,h,depth,input_format,type,pixels);
}
void glTexImage2D(GLenum target,GLint level,GLint format,GLsizei w,GLsizei h,GLint border,GLenum input_format,GLenum type,const void* pixels) {
    glTexImage3D(target,level,format,w,h,1,border,input_format,type,pixels);
}
void glTexSubImage3D(GLenum target,GLint level,GLint x,GLint y,GLint z,GLsizei w,GLsizei h,GLsizei depth,GLenum format,GLenum type,const void* pixels) {
    require(level==0&&x>=0&&y>=0&&z>=0&&w>=0&&h>=0&&depth>=0,"Unsupported texture update");
    auto& s=state();auto& t=bound_texture(target);require(bool(t.resource),"Texture has no storage");
    require(uint64_t(x)+w<=t.resource->desc.Width&&uint64_t(y)+h<=t.resource->desc.Height&&z+depth<=t.resource->desc.DepthOrArraySize,"Texture update out of bounds");
    uint32_t input_bpp=channels(format)*(type==GL_FLOAT?4:type==GL_HALF_FLOAT?2:1);
    const uint8_t* bytes=static_cast<const uint8_t*>(pixels);
    auto unpack=s.bound_buffers.find(GL_PIXEL_UNPACK_BUFFER);
    if(unpack!=s.bound_buffers.end()&&unpack->second) {
        const auto& b=s.buffers.at(unpack->second).bytes;const auto offset=uintptr_t(pixels);
        require(uint64_t(offset)+uint64_t(w)*h*depth*input_bpp<=b.size(),"Pixel unpack buffer overflow");bytes=b.data()+offset;
    }
    require(bytes!=nullptr||!w||!h,"Missing texture pixels");
    std::vector<uint8_t> converted;
    uint32_t output_bpp=input_bpp;
    if(type==GL_UNSIGNED_BYTE&&(format==GL_BGRA||format==GL_RGB)) {
        converted.resize(size_t(w)*h*depth*4);
        for(size_t i=0;i<size_t(w)*h*depth;++i) {
            converted[4*i]=bytes[input_bpp*i+(format==GL_BGRA?2:0)];
            converted[4*i+1]=bytes[input_bpp*i+1];converted[4*i+2]=bytes[input_bpp*i+(format==GL_BGRA?0:2)];
            converted[4*i+3]=input_bpp==4?bytes[input_bpp*i+3]:255;
        }
        bytes=converted.data();output_bpp=4;
    }
    for(int layer=0;layer<depth;++layer) gpu().upload_texture(*t.resource,z+layer,x,y,w,h,bytes+size_t(layer)*w*h*output_bpp,output_bpp);
    gpu().keep_alive(t.resource);
    t.mips_dirty=true;
}
void glTexSubImage2D(GLenum target,GLint level,GLint x,GLint y,GLsizei w,GLsizei h,GLenum format,GLenum type,const void* pixels) {
    glTexSubImage3D(target,level,x,y,0,w,h,1,format,type,pixels);
}
void glGenerateMipmap(GLenum target) {
    bound_texture(target).mip_requested=true;
    bound_texture(target).mips_dirty=true;
}
void glBindImageTexture(GLuint unit,GLuint texture,GLint level,GLboolean layered,GLint layer,GLenum,GLenum) {
    require(level==0&&!layered&&layer==0,"Unsupported image layer");state().image_slots.at(unit)=texture;
}
void glDeleteTextures(GLsizei n,const GLuint* ids) {
    for(int i=0;i<n;++i) {auto it=state().textures.find(ids[i]);if(it==state().textures.end()) continue;
        if(it->second.resource){gpu().keep_alive(it->second.resource);gpu().release_rtv(*it->second.resource);}
        for(auto& id:state().texture_slots)if(id==ids[i])id=0;
        for(auto& id:state().image_slots)if(id==ids[i])id=0;
        state().textures.erase(it);}
}
void glGenFramebuffers(GLsizei n,GLuint* ids) {for(int i=0;i<n;++i){ids[i]=state().next_id++;state().framebuffers.emplace(ids[i],FramebufferState{});}}
void glBindFramebuffer(GLenum target,GLuint id) {require(target==GL_FRAMEBUFFER,"Unsupported framebuffer target");state().framebuffer=id;}
void glFramebufferTexture2D(GLenum,GLenum attachment,GLenum,GLuint texture,GLint level) {
    require(level==0&&attachment>=GL_COLOR_ATTACHMENT0,"Unsupported framebuffer attachment");
    state().framebuffers.at(state().framebuffer).attachments.at(attachment-GL_COLOR_ATTACHMENT0)=texture;
}
GLenum glCheckFramebufferStatus(GLenum) {
    auto& s=state();if(!s.framebuffer) return GL_FRAMEBUFFER_COMPLETE;
    auto& fb=s.framebuffers.at(s.framebuffer);uint64_t width=0;uint32_t height=0;
    for(auto i:fb.draws) {auto id=fb.attachments.at(i);if(!id||!s.textures.at(id).resource) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        const auto& d=s.textures.at(id).resource->desc;if(width&&(d.Width!=width||d.Height!=height)) return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;width=d.Width;height=d.Height;}
    return GL_FRAMEBUFFER_COMPLETE;
}
void glDeleteFramebuffers(GLsizei n,const GLuint* ids) {for(int i=0;i<n;++i){if(state().framebuffer==ids[i])state().framebuffer=0;state().framebuffers.erase(ids[i]);}}
void glDrawBuffers(GLsizei count,const GLenum* targets) {
    if(!state().framebuffer){require(count==1,"Multiple swapchain outputs");return;}
    auto& d=state().framebuffers.at(state().framebuffer).draws;d.clear();for(int i=0;i<count;++i)d.push_back(targets[i]-GL_COLOR_ATTACHMENT0);
}
void glReadBuffer(GLenum mode) {state().read_attachment=mode-GL_COLOR_ATTACHMENT0;}
void glCopyTexSubImage2D(GLenum target,GLint level,GLint xoffset,GLint yoffset,GLint x,GLint y,GLsizei w,GLsizei h) {
    require(level==0,"Unsupported texture mip copy");auto& src=render_target(state().read_attachment);auto& t=bound_texture(target);auto& dst=*t.resource;
    gpu().transition(src,D3D12_RESOURCE_STATE_COPY_SOURCE);gpu().transition(dst,D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION sl{},dl{};sl.pResource=src.object.Get();sl.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dl.pResource=dst.object.Get();dl.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_BOX box{UINT(x),UINT(y),0,UINT(x+w),UINT(y+h),1};
    gpu().commands()->CopyTextureRegion(&dl,xoffset,yoffset,0,&sl,&box);gpu().keep_alive(t.resource);t.mips_dirty=true;
}
void glPixelStorei(GLenum name,GLint value) {require((name==GL_PACK_ALIGNMENT||name==GL_UNPACK_ALIGNMENT)&&value==1,"Unsupported pixel storage layout");}
void glReadPixels(GLint x,GLint y,GLsizei w,GLsizei h,GLenum format,GLenum type,void* output) {
    require(type==GL_UNSIGNED_BYTE&&(format==GL_RGB||format==GL_RGBA),"Unsupported readback layout");
    auto& r=render_target(state().framebuffer?state().read_attachment:0);
    const uint32_t source_bpp=r.desc.Format==DXGI_FORMAT_R8_UNORM?1:4;
    require(source_bpp==1||r.desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM,"Unsupported readback source format");
    auto bytes=gpu().readback(r,source_bpp);
    auto* dst=static_cast<uint8_t*>(output);const auto bpp=channels(format);
    for(int row=0;row<h;++row) for(int col=0;col<w;++col) {
        const int sy=state().framebuffer?y+row:int(r.desc.Height)-1-y-row;
        require(sy>=0&&sy<int(r.desc.Height)&&x+col>=0&&uint64_t(x+col)<r.desc.Width,"Readback rectangle out of bounds");
        auto* pixel=dst+(size_t(row)*w+col)*bpp;
        const auto* src=bytes.data()+(size_t(sy)*r.desc.Width+x+col)*source_bpp;
        if(source_bpp==1){pixel[0]=src[0];pixel[1]=pixel[2]=0;if(bpp==4)pixel[3]=255;}
        else std::memcpy(pixel,src,bpp);
    }
}
void glClearColor(GLclampf r,GLclampf g,GLclampf b,GLclampf a) {state().clear_color={r,g,b,a};}
void glClearBufferfv(GLenum target,GLint index,const GLfloat* color) {
    require(target==GL_COLOR,"Unsupported clear type");auto& r=render_target(index);gpu().transition(r,D3D12_RESOURCE_STATE_RENDER_TARGET);
    gpu().commands()->ClearRenderTargetView(gpu().rtv(r),color,0,nullptr);
}
void glClear(GLbitfield bits) {
    require(bits==GL_COLOR_BUFFER_BIT,"Unsupported clear mask");
    if(!state().framebuffer)glClearBufferfv(GL_COLOR,0,state().clear_color.data());
    else for(auto i:state().framebuffers.at(state().framebuffer).draws)glClearBufferfv(GL_COLOR,i,state().clear_color.data());
}
void glEnable(GLenum cap) {require(cap==GL_BLEND,"Unsupported enabled raster state");state().blend_enabled=true;}
void glDisable(GLenum cap) {if(cap==GL_BLEND)state().blend_enabled=false;else require(cap==GL_CULL_FACE||cap==GL_DEPTH_TEST||cap==GL_STENCIL_TEST,"Unsupported disabled raster state");}
void glBlendEquation(GLenum mode) {require(mode==GL_FUNC_ADD,"Unsupported blend equation");}
void glBlendFuncSeparatei(GLuint i,GLenum sc,GLenum dc,GLenum sa,GLenum da) {state().blend.at(i)={sc,dc,sa,da};}
void glBlendFuncSeparate(GLenum sc,GLenum dc,GLenum sa,GLenum da) {for(auto& b:state().blend)b={sc,dc,sa,da};}
void glViewport(GLint x,GLint y,GLsizei w,GLsizei h) {auto& s=state();s.viewport_x=x;s.viewport_y=y;s.viewport_width=w;s.viewport_height=h;}
GLuint glCreateShader(GLenum type) {
    auto id=state().next_id++;ShaderState shader;
    shader.stage=type==GL_VERTEX_SHADER?Stage::Vertex:type==GL_FRAGMENT_SHADER?Stage::Pixel:Stage::Compute;
    state().shaders.emplace(id,std::move(shader));return id;
}
void glShaderSource(GLuint id,GLsizei count,const GLchar* const* text,const GLint* lengths) {
    auto& s=state().shaders.at(id);s.source.clear();for(int i=0;i<count;++i)s.source.append(text[i],lengths&&lengths[i]>=0?size_t(lengths[i]):strlen(text[i]));
}
void glCompileShader(GLuint id) {
    const auto began=diag::enabled()?diag::ticks():0;
    auto& s=state().shaders.at(id);
    try{s.normal=compile_shader(s.source,s.stage,false,"D2GL DX12 shader");if(s.stage==Stage::Vertex)s.flipped=compile_shader(s.source,s.stage,true,"D2GL DX12 vertex");s.valid=true;}
    catch(const std::exception& e){s.error=e.what();s.valid=false;OutputDebugStringA(s.error.c_str());}
    if(began)diag::note(s.valid?"shader_compile_us":"shader_compile_failed_us",int64_t(diag::milliseconds(diag::ticks()-began)*1000));
}
void glGetShaderiv(GLuint id,GLenum name,GLint* result) {auto& s=state().shaders.at(id);*result=name==GL_COMPILE_STATUS?s.valid:name==GL_INFO_LOG_LENGTH?GLint(s.error.size()+1):0;}
void glGetShaderInfoLog(GLuint id,GLsizei size,GLsizei* len,GLchar* log) {
    const auto& msg=state().shaders.at(id).error;const auto n=std::min(size_t(std::max(0,size-1)),msg.size());
    if(size>0){memcpy(log,msg.data(),n);log[n]=0;}if(len)*len=GLsizei(n);
}
void glDeleteShader(GLuint id) {state().shaders.erase(id);}
GLuint glCreateProgram() {auto id=state().next_id++;state().programs.emplace(id,ProgramState{});return id;}
void glAttachShader(GLuint program,GLuint shader) {if(shader)state().programs.at(program).shaders.push_back(state().shaders.at(shader));}
void glLinkProgram(GLuint) {}
void glValidateProgram(GLuint) {}
void glUseProgram(GLuint id) {state().program=id;}
void glDeleteProgram(GLuint id) {gpu().wait_idle();state().programs.erase(id);}
GLint glGetUniformLocation(GLuint program,const GLchar* name) {
    auto& p=state().programs.at(program);auto it=std::find(p.locations.begin(),p.locations.end(),name);
    if(it!=p.locations.end())return GLint(it-p.locations.begin());
    p.locations.push_back(name);return GLint(p.locations.size()-1);
}
GLuint glGetUniformBlockIndex(GLuint program,const GLchar* name) {
    auto& p=state().programs.at(program);auto it=std::find(p.blocks.begin(),p.blocks.end(),name);
    if(it!=p.blocks.end())return GLuint(it-p.blocks.begin());
    p.blocks.push_back(name);return GLuint(p.blocks.size()-1);
}
void glUniformBlockBinding(GLuint program,GLuint index,GLuint binding) {auto& p=state().programs.at(program);p.block_bindings[p.blocks.at(index)]=binding;++p.version;}
void set_uniform(GLint location,const void* data,size_t bytes) {
    if(location<0)return;auto& p=state().programs.at(state().program);auto& value=p.values[p.locations.at(location)];
    if(value.size()==bytes&&!std::memcmp(value.data(),data,bytes))return;
    value.resize(bytes);std::memcpy(value.data(),data,bytes);++p.version;
}
void glUniform1i(GLint p,GLint v){set_uniform(p,&v,4);}
void glUniform1ui(GLint p,GLuint v){set_uniform(p,&v,4);}
void glUniform1f(GLint p,GLfloat v){set_uniform(p,&v,4);}
void glUniform2fv(GLint p,GLsizei n,const GLfloat* v){set_uniform(p,v,size_t(n)*8);}
void glUniform4fv(GLint p,GLsizei n,const GLfloat* v){set_uniform(p,v,size_t(n)*16);}
void glUniformMatrix4fv(GLint p,GLsizei n,GLboolean trans,const GLfloat* v){require(!trans,"Transposed matrix unsupported");set_uniform(p,v,size_t(n)*64);}
void glDrawElements(GLenum mode,GLsizei count,GLenum type,const void* index){require(mode==GL_TRIANGLES,"Only triangle primitives supported");draw_indexed(count,type,index,0);}
void glDrawElementsBaseVertex(GLenum mode,GLsizei count,GLenum type,void* index,GLint base){require(mode==GL_TRIANGLES,"Only triangle primitives supported");draw_indexed(count,type,index,base);}
void glDispatchCompute(GLuint x,GLuint y,GLuint z){dispatch_compute(x,y,z);}
void glMemoryBarrier(GLbitfield){D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;gpu().commands()->ResourceBarrier(1,&barrier);}
}
