# Build MXL Smooth Motion

## Renderer release

On Windows, install Visual Studio 2022 C++ Build Tools with a Windows SDK, plus Python 3.

From the repository root:

```powershell
.\build.ps1
python .\scripts\package.py
```

The build runs the embedded timing tests and builds the Release/Win32 Glide and DirectDraw targets. It does not install into a game directory. The package script verifies the x86 DLLs, matching MPQ and archive contents, and writes `dist/mxl-smooth-motion-1.0.zip`.

DLL outputs are `build/bin/glide3x.dll` and `build/bin/ddraw.dll`. Vendored build libraries, asset archive and license notices are included in source. The normal runtime engine is the official D2FPS obtained by the installer or Median XL launcher, not a second custom FPS DLL.

Release downloads also include a corresponding source ZIP and SHA-256 checksums.

## Optional D2FPS development build

The full Rust source workspace is under `d2fps/`. It retains the source-level version of the multiplayer timing correction for development and comparison.

```powershell
.\d2fps\build-mxl.ps1
```

This requires Rust/rustup and the MSVC tools. The workspace pins its compiler and dependency versions. It runs seven release-mode tests, then builds the optional custom DLL. That output is not the runtime DLL in the standard MXL Smooth Motion package.

See the [architecture](ARCHITECTURE.md), [source versions](UPSTREAM.json) and [validation record](VALIDATION.md).
