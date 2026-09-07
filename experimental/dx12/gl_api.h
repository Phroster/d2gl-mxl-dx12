#pragma once
#include <GL/glew.h>
#include <memory>
#include "device.h"
#undef glActiveTexture
#undef glAttachShader
#undef glBindBuffer
#undef glBindBufferRange
#undef glBindFramebuffer
#undef glBindImageTexture
#undef glBindTexture
#undef glBindVertexArray
#undef glBlendEquation
#undef glBlendFuncSeparate
#undef glBlendFuncSeparatei
#undef glBufferData
#undef glBufferSubData
#undef glCheckFramebufferStatus
#undef glClear
#undef glClearBufferfv
#undef glClearColor
#undef glCompileShader
#undef glCopyTexSubImage2D
#undef glCreateProgram
#undef glCreateShader
#undef glDeleteBuffers
#undef glDeleteFramebuffers
#undef glDeleteProgram
#undef glDeleteShader
#undef glDeleteTextures
#undef glDeleteVertexArrays
#undef glDisable
#undef glDisableVertexAttribArray
#undef glDispatchCompute
#undef glDrawBuffers
#undef glDrawElements
#undef glDrawElementsBaseVertex
#undef glEnable
#undef glEnableVertexAttribArray
#undef glFramebufferTexture2D
#undef glGenBuffers
#undef glGenFramebuffers
#undef glGenTextures
#undef glGenVertexArrays
#undef glGenerateMipmap
#undef glGetShaderInfoLog
#undef glGetShaderiv
#undef glGetUniformBlockIndex
#undef glGetUniformLocation
#undef glLinkProgram
#undef glMemoryBarrier
#undef glPixelStorei
#undef glReadBuffer
#undef glReadPixels
#undef glShaderSource
#undef glTexImage2D
#undef glTexImage3D
#undef glTexParameteri
#undef glTexSubImage2D
#undef glTexSubImage3D
#undef glUniform1f
#undef glUniform1i
#undef glUniform1ui
#undef glUniform2fv
#undef glUniform4fv
#undef glUniformBlockBinding
#undef glUniformMatrix4fv
#undef glUseProgram
#undef glValidateProgram
#undef glVertexAttribIPointer
#undef glVertexAttribPointer
#undef glViewport
namespace mxl::dx12 {
void initialize(HWND window);
void shutdown();
Device& gpu();
void present(bool vsync);
void resize(uint32_t width,uint32_t height);
uint32_t live_validation_errors();
void glActiveTexture(GLenum texture);
void glAttachShader(GLuint program, GLuint shader);
void glBindBuffer(GLenum target, GLuint buffer);
void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
void glBindFramebuffer(GLenum target, GLuint framebuffer);
void glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
void glBindTexture(GLenum target, GLuint texture);
void glBindVertexArray(GLuint array);
void glBlendEquation(GLenum mode);
void glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
void glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
GLenum glCheckFramebufferStatus(GLenum target);
void glClear(GLbitfield mask);
void glClearBufferfv(GLenum buffer, GLint drawBuffer, const GLfloat* value);
void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void glCompileShader(GLuint shader);
void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GLuint glCreateProgram(void);
GLuint glCreateShader(GLenum type);
void glDeleteBuffers(GLsizei n, const GLuint* buffers);
void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
void glDeleteProgram(GLuint program);
void glDeleteShader(GLuint shader);
void glDeleteTextures(GLsizei n, const GLuint *textures);
void glDeleteVertexArrays(GLsizei n, const GLuint* arrays);
void glDisable(GLenum cap);
void glDisableVertexAttribArray(GLuint index);
void glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
void glDrawBuffers(GLsizei n, const GLenum* bufs);
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
void glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, void *indices, GLint basevertex);
void glEnable(GLenum cap);
void glEnableVertexAttribArray(GLuint index);
void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void glGenBuffers(GLsizei n, GLuint* buffers);
void glGenFramebuffers(GLsizei n, GLuint* framebuffers);
void glGenTextures(GLsizei n, GLuint *textures);
void glGenVertexArrays(GLsizei n, GLuint* arrays);
void glGenerateMipmap(GLenum target);
void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
void glGetShaderiv(GLuint shader, GLenum pname, GLint* param);
GLuint glGetUniformBlockIndex(GLuint program, const GLchar* uniformBlockName);
GLint glGetUniformLocation(GLuint program, const GLchar* name);
void glLinkProgram(GLuint program);
void glMemoryBarrier(GLbitfield barriers);
void glPixelStorei(GLenum pname, GLint param);
void glReadBuffer(GLenum mode);
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
void glShaderSource(GLuint shader, GLsizei count, const GLchar *const* string, const GLint* length);
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
void glTexImage3D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
void glUniform1f(GLint location, GLfloat v0);
void glUniform1i(GLint location, GLint v0);
void glUniform1ui(GLint location, GLuint v0);
void glUniform2fv(GLint location, GLsizei count, const GLfloat* value);
void glUniform4fv(GLint location, GLsizei count, const GLfloat* value);
void glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void glUseProgram(GLuint program);
void glValidateProgram(GLuint program);
void glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void*pointer);
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
}
#ifndef MXL_DX12_API_IMPLEMENTATION
#define glActiveTexture ::mxl::dx12::glActiveTexture
#define glAttachShader ::mxl::dx12::glAttachShader
#define glBindBuffer ::mxl::dx12::glBindBuffer
#define glBindBufferRange ::mxl::dx12::glBindBufferRange
#define glBindFramebuffer ::mxl::dx12::glBindFramebuffer
#define glBindImageTexture ::mxl::dx12::glBindImageTexture
#define glBindTexture ::mxl::dx12::glBindTexture
#define glBindVertexArray ::mxl::dx12::glBindVertexArray
#define glBlendEquation ::mxl::dx12::glBlendEquation
#define glBlendFuncSeparate ::mxl::dx12::glBlendFuncSeparate
#define glBlendFuncSeparatei ::mxl::dx12::glBlendFuncSeparatei
#define glBufferData ::mxl::dx12::glBufferData
#define glBufferSubData ::mxl::dx12::glBufferSubData
#define glCheckFramebufferStatus ::mxl::dx12::glCheckFramebufferStatus
#define glClear ::mxl::dx12::glClear
#define glClearBufferfv ::mxl::dx12::glClearBufferfv
#define glClearColor ::mxl::dx12::glClearColor
#define glCompileShader ::mxl::dx12::glCompileShader
#define glCopyTexSubImage2D ::mxl::dx12::glCopyTexSubImage2D
#define glCreateProgram ::mxl::dx12::glCreateProgram
#define glCreateShader ::mxl::dx12::glCreateShader
#define glDeleteBuffers ::mxl::dx12::glDeleteBuffers
#define glDeleteFramebuffers ::mxl::dx12::glDeleteFramebuffers
#define glDeleteProgram ::mxl::dx12::glDeleteProgram
#define glDeleteShader ::mxl::dx12::glDeleteShader
#define glDeleteTextures ::mxl::dx12::glDeleteTextures
#define glDeleteVertexArrays ::mxl::dx12::glDeleteVertexArrays
#define glDisable ::mxl::dx12::glDisable
#define glDisableVertexAttribArray ::mxl::dx12::glDisableVertexAttribArray
#define glDispatchCompute ::mxl::dx12::glDispatchCompute
#define glDrawBuffers ::mxl::dx12::glDrawBuffers
#define glDrawElements ::mxl::dx12::glDrawElements
#define glDrawElementsBaseVertex ::mxl::dx12::glDrawElementsBaseVertex
#define glEnable ::mxl::dx12::glEnable
#define glEnableVertexAttribArray ::mxl::dx12::glEnableVertexAttribArray
#define glFramebufferTexture2D ::mxl::dx12::glFramebufferTexture2D
#define glGenBuffers ::mxl::dx12::glGenBuffers
#define glGenFramebuffers ::mxl::dx12::glGenFramebuffers
#define glGenTextures ::mxl::dx12::glGenTextures
#define glGenVertexArrays ::mxl::dx12::glGenVertexArrays
#define glGenerateMipmap ::mxl::dx12::glGenerateMipmap
#define glGetShaderInfoLog ::mxl::dx12::glGetShaderInfoLog
#define glGetShaderiv ::mxl::dx12::glGetShaderiv
#define glGetUniformBlockIndex ::mxl::dx12::glGetUniformBlockIndex
#define glGetUniformLocation ::mxl::dx12::glGetUniformLocation
#define glLinkProgram ::mxl::dx12::glLinkProgram
#define glMemoryBarrier ::mxl::dx12::glMemoryBarrier
#define glPixelStorei ::mxl::dx12::glPixelStorei
#define glReadBuffer ::mxl::dx12::glReadBuffer
#define glReadPixels ::mxl::dx12::glReadPixels
#define glShaderSource ::mxl::dx12::glShaderSource
#define glTexImage2D ::mxl::dx12::glTexImage2D
#define glTexImage3D ::mxl::dx12::glTexImage3D
#define glTexParameteri ::mxl::dx12::glTexParameteri
#define glTexSubImage2D ::mxl::dx12::glTexSubImage2D
#define glTexSubImage3D ::mxl::dx12::glTexSubImage3D
#define glUniform1f ::mxl::dx12::glUniform1f
#define glUniform1i ::mxl::dx12::glUniform1i
#define glUniform1ui ::mxl::dx12::glUniform1ui
#define glUniform2fv ::mxl::dx12::glUniform2fv
#define glUniform4fv ::mxl::dx12::glUniform4fv
#define glUniformBlockBinding ::mxl::dx12::glUniformBlockBinding
#define glUniformMatrix4fv ::mxl::dx12::glUniformMatrix4fv
#define glUseProgram ::mxl::dx12::glUseProgram
#define glValidateProgram ::mxl::dx12::glValidateProgram
#define glVertexAttribIPointer ::mxl::dx12::glVertexAttribIPointer
#define glVertexAttribPointer ::mxl::dx12::glVertexAttribPointer
#define glViewport ::mxl::dx12::glViewport
#endif
