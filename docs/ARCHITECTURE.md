# MXL Smooth Motion DX12 architecture

The main renderer converts the game's Glide or DirectDraw calls into native Direct3D 12 work. Both wrapper DLLs contain the same multiplayer timing correction.

## Rendering

- DX12 textures, buffers, pipeline states, command lists and resource barriers.
- Reusable upload memory, fenced resource lifetimes and cached binding state.
- DXGI flip presentation, a short frame queue, and V-Sync/tearing support.
- D2GL's original graphics options and assets, including the unchanged MPQ.
- Native DX12 rendering for Dear ImGui and the Ctrl+O menu.
- An FPS tab that saves D2FPS settings for the next game launch.

Shader sources are translated through glslang and SPIRV-Cross, then compiled to DirectX bytecode. The source's internal OpenGL-shaped calls are implemented by the DX12 backend; no OpenGL driver context is created.

## Timing and D2FPS

The official `d2fps.dll` stays on disk and supplies the FPS engine. The renderer loads it and applies the guarded correction in memory. This preserves the Median XL launcher's normal D2FPS file check.

The correction replaces five clock operands and extends the upper interpolation bound only for realm multiplayer. The original 40 ms simulation interval, negative-time path, SP/LAN bound and authoritative position restoration are retained.

Exact supported SHA-256 hashes:

| File | SHA-256 |
|---|---|
| D2Client.dll | `dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906` |
| Official d2fps.dll | `db9de4d4d320a7b70e66fe6b4aaa0e6f1560a5300a4993cc81cf4512ab1240c1` |

D2Sigma must be loaded. File identity, patch instructions, clock imports and the simulation interval are checked before applying the fix. The status reports On after successful application and readback.

## Improvements in 1.1

The renderer uploads each buffer's current live range once per frame/version instead of repeatedly copying growing prefixes for later draws. Partial updates retain the untouched tail. This removes the excessive upload/spill work measured during crowded fights.

Automatic act reveal calls Median XL's original routine on the game thread through a queued window message after a completed gameplay frame. It validates the current player/act/room state and honors Median XL's own completion flags. It moves the reveal cost to act entry; it does not accelerate level generation. D2Sigma and D2Common file hashes and code signatures must match the supported build.

Performance recording is off unless `[Diagnostics] enabled=1` is explicitly set in `mxl-diagnostics.ini`. With recording off, no diagnostic writer, audio timing hooks, reveal timing detours or GPU timestamp queries are started. The upload fix and guarded automatic reveal remain active. A process-wide owner prevents duplicate reveal initialization across the two renderer DLLs.

## Launcher and ReShade

Enable both custom graphics DLL checkboxes under the launcher's **Unofficial Graphic Drivers** section. The unchanged MPQ matches the inspected official distribution.

ReShade uses its DirectX installation. ReShade 6.6.2.2082 was verified locally through `dxgi.dll`, retaining the existing preset. ReShade and personal presets are not bundled with the mod.

The `d2fps/` source workspace remains for development/reference. It is not loaded as a second FPS engine.

[Build](BUILD.md) · [Validation](VALIDATION.md) · [Upstream versions](UPSTREAM.json)
