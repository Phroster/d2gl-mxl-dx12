# Build MXL Smooth Motion DX12

Install Visual Studio 2022 C++ Build Tools with a Windows SDK, CMake and Git.

From the repository root:

```powershell
.\build.ps1 -BuildDirectory 'C:\MXL-DX12-Build'
python .\scripts\package.py --build-dir 'C:\MXL-DX12-Build'
```

The build fetches pinned shader compiler sources, builds both x86 renderer DLLs and runs the native shader, timing, GPU and menu checks. It does not install files into a game. Add `-SkipGpuTests` when building without a usable DX12 GPU.

The DLLs are in the build directory's `Release` folder. Packaging writes `dist/mxl-smooth-motion-dx12-1.0.zip`.

Use the CMake build for DX12. The imported Visual Studio project files describe the original renderer. The internal `experimental/` directory retains its historical name; its DX12 implementation is the main renderer.

[Dependency versions](../experimental/DEPENDENCIES.json) · [Architecture](ARCHITECTURE.md) · [Validation](VALIDATION.md)

## Accepted 1.0 binaries

The promoted 1.0 download keeps the exact renderer binaries accepted during the DX12 checkpoint. Their Windows file metadata still identifies the original 0.1 build. The package/release is version 1.0; documentation and repository promotion do not change the rendering code.

The package manifest records the file hashes and source revision. The release includes a corresponding source archive.

## D2FPS source

The `d2fps/` workspace retains the source-level timing implementation for development and comparison. The main package runs Median XL's official D2FPS, with the correction applied by the DX12 renderer. That official DLL is not redistributed here.
