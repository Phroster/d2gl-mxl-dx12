# Licensing and attribution

The D2GL-based renderer code is distributed under **GNU GPL version 3 or later (`GPL-3.0-or-later`)**. The full license is in [LICENSE](../LICENSE). D2GL's copyright notices, including Copyright (C) 2023 Bayaraa, and its original [license](../d2gl/LICENSE.md) remain in the source. MXL Smooth Motion DX12 is a modified version of D2GL; this fork adds the DirectX 12 renderer and Median XL integration. The 1.0 redistribution notices were updated on 2026-09-07.

Third-party code and assets retain their respective terms. The renderer's GPL statement does not relicense every file or asset in the repository. Original source notices and the matching MPQ's embedded notices are preserved.

## Player download

The player ZIP contains the game files and one consolidated `LICENSES.txt`. That file carries the full GPL text, applicable component copyright and license notices, and links to the exact source revision and build instructions. Installation guides, development files and the packaging manifest stay outside the player ZIP.

[player-notices.json](../licenses/player-notices.json) lists the text included by the packager. The pinned glslang and SPIRV-Cross license copies are checked against the dependencies used for the build. The [original D2GL third-party notice file](../d2gl/THIRD_PARTY_LICENSES.md) remains in the source tree for provenance.

## Components built into the renderer

| Component | Terms and retained notices |
|---|---|
| D2GL and D2DX motion predictor code | GPL-3.0-or-later; Bayaraa and Bolrog's original notices are retained. |
| Microsoft Detours 4.0.1 | MIT; statically linked into the renderer. |
| Dear ImGui 1.89.2 | MIT; the Win32 and DirectX 12 backends are built with the renderer. |
| ProggyClean, embedded in Dear ImGui | MIT; Copyright (c) 2004, 2005 Tristan Grimmer. |
| GLM 0.9.9.8 | MIT option in the upstream dual-license notice. |
| GLEW/OpenGL headers | Original GLEW, Mesa and Khronos permissive notices. The DX12 build does not link a GLEW runtime library. |
| stb image helpers and ImGui's stb helpers | Upstream MIT/public-domain alternatives and notices. |
| 3dfx Glide interface headers | The vendored `COPYING.txt` is retained separately from the renderer's GPL license. |
| glslang | The complete license file from the exact pinned revision, including its component notices. |
| SPIRV-Cross | Apache-2.0 option for the core; MIT and Khronos notices for the included SPIR-V headers. |
| Built-in postprocessing and MPQ shader sources | Original per-component notices, including FXAA, LumaSharpen and the libretro shader contributors. GPL version 2 is also supplied for separately licensed shaders. |

The preserved shader notice index records the copyright and license comments present in the MPQ sources. Individual source notices govern those components; the index does not assign a common license to them.

## Exact source and dependencies

Use the source link on the release page or in the ZIP's `LICENSES.txt` for the revision corresponding to the renderer DLLs. The [build guide](BUILD.md) contains the build commands. GPL source access is available separately from the player ZIP.

| Source | Revision |
|---|---|
| [D2GL Median XL foundation](https://github.com/GavinK88/d2gl-mxl-1.0/tree/6284091f4923b6c5a617b9f4caedac292064aae4) | `6284091f4923b6c5a617b9f4caedac292064aae4` |
| [glslang](https://github.com/DiligentGraphics/glslang/tree/275822a6261ee689aadb1da5f09a0ec2f058685c) | `275822a6261ee689aadb1da5f09a0ec2f058685c` |
| [SPIRV-Cross](https://github.com/DiligentGraphics/SPIRV-Cross/tree/1a6169566c73d3da552748fc372fe2bbb856e46e) | `1a6169566c73d3da552748fc372fe2bbb856e46e` |
| [Microsoft Detours source](https://github.com/microsoft/Detours/tree/v4.0.1) | `v4.0.1`, matching the vendored header's version identifier |

The dependency revisions are defined in [Dependencies.cmake](../cmake/Dependencies.cmake). The Detours library is the existing vendored binary; the version identifier above comes from its accompanying header.

## D2FPS

The player ZIP does not contain `d2fps.dll`. Median XL supplies the official D2FPS engine separately. This fork's integration code is part of the renderer source. Historical D2FPS license files remain in [licenses](../licenses), and [UPSTREAM.json](UPSTREAM.json) links the historical source, but those historical notices are not added to the current player ZIP as if it contained D2FPS.
