# MXL Smooth Motion: DX12 experiment

This local experimental branch replaces the graphics backend while preserving the classic game's Glide/DirectDraw entry points, D2GL's graphics options and assets, and the accepted D2FPS timing correction.

The implementation targets native Direct3D 12 resources, command lists and DXGI presentation. GLSL is a source asset format only: glslang and SPIRV-Cross translate it to HLSL, which is compiled for D3D12. No OpenGL context or OpenGL-to-DXGI presentation bridge is used.

The existing OpenGL-shaped internal calls are adapted to a limited renderer interface implemented in DX12; this is not a general OpenGL driver. Unsupported operations must report a clear error instead of silently drawing nothing.

The stable public 1.0 release and normal game folder remain separate. This branch is not published as a release.

Validation proceeds through native device and shader tests, rendered/readback image tests, both wrapper builds, and an isolated game installation. Performance improvements require in-game evidence; successful compilation alone is not proof.
