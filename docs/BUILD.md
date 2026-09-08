# Build from source

Players can use the [ready-to-play download](../README.md#install--copy-paste-play).

For a source build, install **Visual Studio 2022 C++ Build Tools**, a **Windows SDK**, **CMake 3.24+** and **Git**.

```console
cmake -S . -B C:/MXL-DX12-Build -G "Visual Studio 17 2022" -A Win32
cmake --build C:/MXL-DX12-Build --config Release --target glide3x ddraw --parallel 6
```

CMake downloads the pinned shader compiler sources automatically. The two DLLs are written to `C:/MXL-DX12-Build/Release`. The build does not install anything into a game.

## Run the checks

```console
cmake -S . -B C:/MXL-DX12-Build -DMXL_BUILD_TESTS=ON
cmake --build C:/MXL-DX12-Build --config Release --parallel 6
ctest --test-dir C:/MXL-DX12-Build -C Release --output-on-failure
```

The native checks cover rendering, shaders, the settings menu, movement timing, uploads, automatic reveal, native sound cancellation and optional recording. GPU tests require a working DirectX 12 device. Test output stays in the build directory.

## Create the player download

With Python 3 installed:

```console
python tools/package.py --build-dir C:/MXL-DX12-Build
```

The ZIP contains exactly the six installation files and one consolidated `LICENSES.txt`. Guides, images, source files and build tools stay in the repository. The packaging script checks the complete ZIP inventory, file hashes and disabled recording default. It writes the verification manifest and checksum beside the ZIP, outside the player download.

The notice inventory is maintained in [`licenses/player-notices.json`](../licenses/player-notices.json). Packaging checks the copied compiler license texts against the actual build dependencies. See [licensing and credits](LICENSING.md) for component terms and source references.

## Source layout

| Folder | Contents |
|---|---|
| `d2gl/` | D2GL source, Glide/DirectDraw entry points, matching MPQ and required libraries. |
| `src/dx12/` | DirectX 12 rendering, automatic reveal and optional diagnostics. |
| `tests/` | Native verification programs. |
| `cmake/` | Build setup, dependency revisions and test registration. |
| `tools/` | Packaging, shader asset checks and diagnostic report analysis. |
| `defaults/` | Player graphics and FPS settings. |

The active package uses Median XL's official `d2fps.dll`. Its timing integration is in `d2gl/d2gl/src/mxl_smoothing.c`; the FPS settings menu is in `src/dx12/fps_menu.cpp`.

[Architecture](ARCHITECTURE.md) · [Dependency versions](../cmake/DEPENDENCIES.json) · [Original projects](UPSTREAM.json)
