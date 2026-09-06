# DX12 experiment validation

## Passed

- Native x86 Direct3D 12 device creation on the NVIDIA GeForce RTX 4080.
- GPU colour-target clear and texture upload/readback, including padded rows.
- Textured geometry, matrix uniforms, sampler binding and framebuffer orientation.
- Native GPU mipmap generation, followed by an explicit mip-level sampling check.
- The actual D2GL Glide shader with half-float/integer vertex attributes, R8 texture arrays, palette/gamma constant data and three simultaneous colour outputs.
- Dear ImGui DX12 text rendering and the D2FPS options panel; a GPU-readback image was inspected.
- All 14 built-in shader stages compiled to DirectX shader bytecode.
- All 238 shader stages from 119 shader passes in the supplied MPQ compiled through the Slang/GLSL-to-HLSL path. This is compiler coverage, not a visual check of every preset.
- Both x86 renderer DLLs preserve the stable wrapper's exported names and ordinals. They import D3D12, DXGI and D3DCompiler; neither imports OpenGL.
- The separate Median XL game copy started with the native DX12 adapter and all six original smoothing changes verified. The user reported that the main menu looked normal. Fullscreen/windowed transitions were also recorded in that run.
- Normal-install file hashes remained unchanged during the isolated test.

## Scope of the experiment

Actual single-player and realm-multiplayer gameplay have not yet been confirmed. No numerical performance improvement, universal visual parity or G-Sync behaviour is claimed.

The Windows DX12 debug layer was unavailable on the test machine, so the GPU checks use exact readback results rather than claiming a clean debug-layer run.

The startup log also recorded the legacy D2GL sleep-site mismatch after D2FPS patched that site, and an unavailable optional English atlas file. The main menu appeared normal, but in-game font coverage remains part of gameplay testing.

The original simulation and D2FPS timing correction are retained. This experiment changes the renderer, shader translation and presentation machinery; it does not replace Diablo II's simulation or networking.

## Shader compatibility adjustments

Cross-compilation initializes undefined shader values. The bundled legacy EGA preset additionally receives a narrowly matched correction for a duplicated palette selector and a missing return path; invalid selectors pass the input colour through. The original MPQ remains unchanged.

## Records

Detailed local build/test logs, the readback image and extracted shader fixtures stay in ignored build directories. The isolated game copy and proprietary game files are not part of the source or mod package.
