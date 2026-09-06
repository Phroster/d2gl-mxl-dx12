# MXL Smooth Motion — DX12 experiment 0.1

A native DirectX 12 version of the renderer, with the original multiplayer smoothing fix and the D2GL graphics menu.

**This is a local experiment. Keep your working installation separate.**

## Try it

Use a separate copy of the supported Median XL 2.14.0 / Diablo II 1.13c game.

1. Close that copy of the game.
2. Copy the experiment's `glide3x.dll`, `ddraw.dll`, `d2gl.mpq`, `d2gl.ini` and `d2fps.ini` into it.
3. Keep the official `d2fps.dll` from Median XL.
4. Start `Game.exe -3dfx -log`, without `-w`.

The experiment starts in a window. Alt+Enter changes fullscreen mode.

Press **Ctrl+O** for the normal graphics options. The new **FPS** tab edits D2FPS settings; save them and restart the game to apply them. The multiplayer timing fix remains automatic.

If you use the Median XL launcher with this copy, enable both custom graphics DLL checkboxes under **Unofficial Graphic Drivers**. Keep the launcher pointed at your usual install unless you intentionally want it to manage this test copy.

## What changed?

- Native DX12 textures, buffers, drawing commands, shaders and presentation.
- GPU fences and reusable upload memory in place of the old per-frame GPU wait.
- Cached pipelines, resource bindings and samplers.
- DXGI flip presentation with a short frame queue and V-Sync/tearing support.
- Native DX12 rendering for the menu, plus the FPS settings tab.
- Existing shader assets and the MPQ retained.

OpenGL ReShade files are not part of this experiment. They cannot provide their OpenGL effects to a DX12 renderer.

For ReShade, use its DirectX installation for this game. The checkpoint was verified with ReShade 6.6.2.2082 loaded as `dxgi.dll`, keeping the existing preset. ReShade is not bundled with the experiment.

The game still updates its simulation at the original rate. This changes how frames are drawn, not the game's speed.

## Build

Install Visual Studio 2022 C++ Build Tools with a Windows SDK, CMake and Git. From the source root:

```powershell
.\experimental\build.ps1 -BuildDirectory 'C:\MXL-DX12-Build'
```

The script fetches pinned shader compiler sources, builds both x86 DLLs and runs the native checks. It does not install into a game. `-SkipGpuTests` permits a build on a machine without a usable DX12 GPU.

Outputs are under the build directory's `Release` folder. `experimental/package.py` creates a mod ZIP from those outputs.

[Validation and limitations](VALIDATION.md) · [Dependency versions](DEPENDENCIES.json)

Based on D2GL by Bayaraa, the MXL adaptations by Pooquer/GavinK88, and D2FPS by Jarcho. Original licenses are retained. Experimental integration by Phroster.
