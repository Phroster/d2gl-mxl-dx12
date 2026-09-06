# MXL Smooth Motion: DX12 experiment

This experimental branch replaces the graphics backend while preserving the classic game's Glide/DirectDraw entry points, D2GL's graphics options and assets, and the accepted D2FPS timing correction.

The implementation targets native Direct3D 12 resources, command lists and DXGI presentation. GLSL is a source asset format only: glslang and SPIRV-Cross translate it to HLSL, which is compiled for D3D12. No OpenGL context or OpenGL-to-DXGI presentation bridge is used.

The existing OpenGL-shaped internal calls are adapted to a limited renderer interface implemented in DX12; this is not a general OpenGL driver. Unsupported operations must report a clear error instead of silently drawing nothing.

The stable public 1.0 release remains unchanged. After accepting the experiment, the owner installed DX12 into the main local game folder and switched the existing ReShade installation to DirectX. This branch is a checkpoint, not a replacement stable release.

The [2026-09-06 checkpoint](experimental/checkpoints/2026-09-06.json) records the tested renderer hashes and the redacted startup results.

Validation proceeds through native device and shader tests, rendered/readback image tests, both wrapper builds, and an isolated game installation. Performance improvements require in-game evidence; successful compilation alone is not proof.
