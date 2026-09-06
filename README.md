# MXL Smooth Motion — DX12 experiment 0.1

This branch contains a **native DirectX 12 renderer** for the supported Median XL / Diablo II 1.13c setup. It keeps the D2GL graphics menu, adds a D2FPS settings tab, and retains the original multiplayer timing fix.

**[Experiment instructions](experimental/README.md)** · [Validation](experimental/VALIDATION.md) · [Design](DX12-EXPERIMENT.md)

The normal public 1.0 release is separate. This experiment has not been published as a replacement.

## Build

Install Visual Studio 2022 C++ Build Tools, a Windows SDK, CMake and Git, then run:

```powershell
.\build.ps1 -BuildDirectory 'C:\MXL-DX12-Build'
```

This fetches pinned shader compiler sources and builds both x86 renderer DLLs and verification programs. It does not install files into a game. Use the CMake build for this branch; the imported Visual Studio project files document the original renderer.

To create a mod-only ZIP:

```powershell
python .\experimental\package.py --build-dir 'C:\MXL-DX12-Build'
```

Use a separate game copy for testing. The package includes the renderer DLLs, unchanged MPQ, INI defaults and license notices; it uses Median XL's official D2FPS.

## What has been checked?

The native GPU image tests, actual Glide shader test, menu test, original timing tests and shader compilation checks passed. The isolated game started successfully and the user confirmed its main menu looked normal. Gameplay and performance still need testing.

Original D2GL, MXL adaptations and D2FPS credits and licenses are retained. Experimental integration by Phroster.
