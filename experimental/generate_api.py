from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
glew=(root/"d2gl/d2gl/vendor/include/GL/glew.h").read_text(encoding="utf-8")
names="""glActiveTexture glAttachShader glBindBuffer glBindBufferRange glBindFramebuffer glBindImageTexture glBindTexture glBindVertexArray glBlendEquation glBlendFuncSeparate glBlendFuncSeparatei glBufferData glBufferSubData glCheckFramebufferStatus glClear glClearBufferfv glClearColor glCompileShader glCopyTexSubImage2D glCreateProgram glCreateShader glDeleteBuffers glDeleteFramebuffers glDeleteProgram glDeleteShader glDeleteTextures glDeleteVertexArrays glDisable glDisableVertexAttribArray glDispatchCompute glDrawBuffers glDrawElements glDrawElementsBaseVertex glEnable glEnableVertexAttribArray glFramebufferTexture2D glGenBuffers glGenFramebuffers glGenTextures glGenVertexArrays glGenerateMipmap glGetShaderInfoLog glGetShaderiv glGetUniformBlockIndex glGetUniformLocation glLinkProgram glMemoryBarrier glPixelStorei glReadBuffer glReadPixels glShaderSource glTexImage2D glTexImage3D glTexParameteri glTexSubImage2D glTexSubImage3D glUniform1f glUniform1i glUniform1ui glUniform2fv glUniform4fv glUniformBlockBinding glUniformMatrix4fv glUseProgram glValidateProgram glVertexAttribIPointer glVertexAttribPointer glViewport""".split()
decl=[]
for name in names:
    alias="PFN"+name.upper()+"PROC"
    found=re.search(r"typedef\s+([^;\n]+?)\s*\(GLAPIENTRY\s*\*\s*"+alias+r"\)\s*\(([^;]+)\);",glew)
    if not found:
        found=re.search(r"GLAPI\s+([^;\n]+?)\s+GLAPIENTRY\s+"+name+r"\s*\(([^;]+)\);",glew)
    if not found: raise RuntimeError("Signature missing: "+name)
    result,args=found.groups()
    decl.append(result.strip()+" "+name+"("+args.strip()+");")
out=["#pragma once","#include <GL/glew.h>","#include <memory>",'#include "device.h"']
out += ["#undef "+n for n in names]
out += ["namespace mxl::dx12 {","void initialize(HWND window);","void shutdown();","Device& gpu();","void present(bool vsync);","void resize(uint32_t width,uint32_t height);","uint32_t live_validation_errors();"]
out += decl
out += ["}"]
out += ["#ifndef MXL_DX12_API_IMPLEMENTATION"]
out += ["#define "+n+" ::mxl::dx12::"+n for n in names]
out += ["#endif"]
(root/"experimental/dx12/gl_api.h").write_text("\n".join(out)+"\n",encoding="utf-8")
print("Generated",len(names),"internal API declarations.")
