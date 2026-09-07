# MXL Smooth Motion DX12 design

DX12 is the main renderer. The public repository uses one `main` branch; earlier implementation stages remain available through commit history and checkpoint tags.

The renderer preserves the classic game's Glide/DirectDraw entry points, D2GL's graphics options and assets, and the accepted D2FPS timing correction.

Rendering uses native Direct3D 12 resources, command lists and DXGI presentation. GLSL is a source asset format: glslang and SPIRV-Cross translate it to HLSL for DirectX compilation. No OpenGL context or OpenGL-to-DXGI presentation bridge is used.

The existing internal graphics calls are implemented through a limited DX12 interface for D2GL. This is not a general OpenGL driver.

The owner accepted the DX12 version after testing it, made it the main local installation, and verified ReShade on DirectX. The [original DX12 checkpoint](experimental/checkpoints/2026-09-06.json) preserves the renderer hashes and startup evidence.

See [architecture](docs/ARCHITECTURE.md), [validation](docs/VALIDATION.md) and [build instructions](docs/BUILD.md).
