from pathlib import Path
root=Path(__file__).resolve().parents[1]
def edit(name,old,new):
    p=root/name
    text=p.read_text(encoding="utf-8-sig")
    assert old in text,name
    p.write_text(text.replace(old,new),encoding="utf-8",newline="\n")
edit("d2gl/d2gl/src/pch.h","#include <GL/glew.h>\n#include <GL/wglew.h>",'#include "gl_api.h"\n#include <atomic>')
p=root/"d2gl/d2gl/src/graphic/context.cpp"
s=p.read_text(encoding="utf-8")
s=s.replace("#include <imgui/imgui_impl_opengl3.h>","#include <imgui/imgui_impl_dx12.h>")
begin=s.index("\tPIXELFORMATDESCRIPTOR pfd;")
end=s.index("\tglDisable(GL_CULL_FACE);",begin)
s=s[:begin]+'''    mxl::dx12::initialize(App.hwnd);
    App.gl_ver = {4, 5}; // Shader-source language level, not an OpenGL context.
    App.gl_ver_str = "DirectX 12";
    App.gl_caps.compute_shader = App.use_compute_shader;
    App.gl_caps.independent_blending = true;
    trace_log("Native DirectX 12 adapter: %s", mxl::dx12::gpu().adapter_name().c_str());
'''+s[end:]
s=s.replace("\twglMakeCurrent(NULL, NULL);\n\tCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Context::renderThread, reinterpret_cast<void*>(this), 0, NULL);",
'''    mxl::dx12::gpu().flush(true);
    m_render_thread = CreateThread(NULL, 0, Context::renderThread, this, 0, NULL);
    if (!m_render_thread) throw std::runtime_error("DX12 render thread could not start.");''')
s=s.replace('''\tfor (uint32_t i = 0; i < MAX_FRAME_LATENCY; i++)
\t\tWaitForSingleObject(m_semaphore_gpu[i], INFINITE);

\twglMakeCurrent(App.hdc, m_context);''',
'''    if (m_render_thread) {
        WaitForSingleObject(m_render_thread, INFINITE);
        CloseHandle(m_render_thread);
    }
    mxl::dx12::gpu().wait_idle();''')
s=s.replace('''\twglMakeCurrent(NULL, NULL);
\twglDeleteContext(m_context);''','''    for (uint32_t i = 0; i < MAX_FRAME_LATENCY; ++i) {
        CloseHandle(m_semaphore_cpu[i]);
        CloseHandle(m_semaphore_gpu[i]);
    }''')
s=s.replace("void Context::renderThread(void* context)","DWORD WINAPI Context::renderThread(void* context)")
s=s.replace("\twglMakeCurrent(App.hdc, ctx->m_context);","")
s=s.replace("\t\tWaitForSingleObject(ctx->m_semaphore_cpu[frame_index], INFINITE);",
'''        WaitForSingleObject(ctx->m_semaphore_cpu[frame_index], INFINITE);
        if (!ctx->m_rendering) break;''')
s=s.replace('''\t\tGLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
\t\tglFlush();
\t\tglClientWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
\t\tglDeleteSync(sync);
''',"")
s=s.replace("\t\tSwapBuffers(App.hdc);","        mxl::dx12::present(App.vsync);")
s=s.replace('''\twglMakeCurrent(NULL, NULL);
\tfor (uint32_t i = 0; i < 2; i++)
\t\tReleaseSemaphore(ctx->m_semaphore_gpu[i], 1, NULL);''',
'''    mxl::dx12::gpu().flush(false);
    for (uint32_t i = 0; i < MAX_FRAME_LATENCY; i++)
        ReleaseSemaphore(ctx->m_semaphore_gpu[i], 1, NULL);
    return 0;''')
s=s.replace("\twglSwapIntervalEXT(App.vsync);","    // V-Sync is selected by DXGI Present for each frame.")
pos=s.index("void Context::onResize")
brace=s.index("{",pos)
s=s[:brace+1]+"\n    mxl::dx12::resize(w_size.x, w_size.y);"+s[brace+1:]
s=s.replace('''\tImGui_ImplOpenGL3_Init("#version 150");''',
'''    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    mxl::dx12::check(mxl::dx12::gpu().native()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_imgui_heap)), "Create UI font heap");
    if (!ImGui_ImplDX12_Init(mxl::dx12::gpu().native(), mxl::dx12::Device::FrameCount,
        DXGI_FORMAT_R8G8B8A8_UNORM, m_imgui_heap.Get(),
        m_imgui_heap->GetCPUDescriptorHandleForHeapStart(), m_imgui_heap->GetGPUDescriptorHandleForHeapStart()))
        throw std::runtime_error("DX12 menu initialization failed.");''')
s=s.replace("ImGui_ImplOpenGL3_Shutdown();","ImGui_ImplDX12_Shutdown();")
s=s.replace("ImGui_ImplOpenGL3_NewFrame();","ImGui_ImplDX12_NewFrame();")
s=s.replace("\tImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());",
'''    auto& backend = mxl::dx12::gpu();
    auto& output = backend.back_buffer();
    backend.transition(output, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto rtv = backend.rtv(output);
    auto* commands = backend.commands();
    commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = {m_imgui_heap.Get()};
    commands->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commands);''')
assert "wgl" not in s and "glFence" not in s and "SwapBuffers" not in s
p.write_text(s,encoding="utf-8",newline="\n")
edit("d2gl/d2gl/src/graphic/context.h","\tHGLRC m_context = nullptr;",
'''    HANDLE m_render_thread = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_imgui_heap;''')
edit("d2gl/d2gl/src/graphic/context.h","\tbool m_rendering = true;","\tstd::atomic_bool m_rendering{true};")
edit("d2gl/d2gl/src/graphic/context.h","static void renderThread(void* context);","static DWORD WINAPI renderThread(void* context);")
edit("d2gl/d2gl/src/app.h",'menu_title = "MXL Smooth Motion"','menu_title = "MXL Smooth Motion - DX12 Experiment"')
edit("d2gl/d2gl/src/option/menu.cpp",'"OpenGL: " + App.gl_ver_str','"Renderer: " + App.gl_ver_str')
p=root/"d2gl/d2gl/vendor/include/imgui/imconfig.h"
p.write_text(p.read_text(encoding="utf-8")+"\n// DX12 GPU descriptors are 64-bit even in the 32-bit game.\n#define ImTextureID ImU64\n",encoding="utf-8")
print("Integrated native DX12 context, menu backend and render-thread lifecycle.")
