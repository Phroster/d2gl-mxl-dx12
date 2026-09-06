# Build and compatibility details

The [player guide](../README.md) contains the normal one-file installation steps. This page keeps implementation and build details out of those instructions.

## Supported game

The added correction activates for Diablo II 1.13c with D2Sigma loaded, D2FPS motion smoothing enabled, and this exact original client:

```text
D2Client.dll SHA-256:
dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906
```

That is the client used with the tested MXL D2Sigma build 20260904. The original-instruction, PE identity, import and 40 ms simulation-interval guards remain in force. An unknown file or unexpected patch site prevents the added correction from activating. Other versions retain upstream timing behavior and its existing compatibility checks.

This is a complete rebuild of public Jarcho/d2-rs commit `177fb5a9d723005ca7fc6c084330997d8c15df51`, not Median XL's signed distribution binary. It may differ from that distribution beyond the timing changes. The build targets modern Windows (Windows 10 or later); upstream's historical support for older Windows versions is not a claim for this binary.

## Release 1.0 and binary identity

Release **1.0** contains the integrated timing correction, simple player instructions and guards based on the actual client instructions. The current package is a clean rebuild: client-code conflicts are checked regardless of module filenames, and the file/startup version labels are aligned. Timing and graphics calculations remain unchanged. This replaces the previous download under the same release version; compare checksums when identifying builds.

- DLL size: 387072 bytes.
- SHA-256: `3b54d5b7c30c4001cfde7e5f8a183db041fb1806182709ddc92f790a7085791f`.
- Windows file version: `1.0.0.0`. Startup label: `D2FPS MXL 1.0: precise clocks and bounded realm prediction`.
- Exports: `_DllMain@12`, `_Init@0`, `_Init@4`.
- Architecture: x86 / PE32; static CRT; unsigned community build.

## Build

On 64-bit Windows, install Rust/rustup, Visual Studio C++ Build Tools and a Windows SDK, then run from the repository root:

```powershell
.\build-mxl.ps1
```

The checked-in configuration selects Rust 1.94.1 MSVC and `i686-pc-windows-msvc`. The script runs release-mode tests, builds the DLL, remaps local source/dependency paths in diagnostics and prints the DLL hash. It does not install anything into a game folder.

Output: `target/i686-pc-windows-msvc/release/d2fps.dll`.

The underlying commands are:

```powershell
cargo test --locked -p d2fps --lib --release
cargo build --locked -p d2fps --release
```

To also compare the native SHA-256 guard with your own original client during tests, set `D2FPS_TEST_CLIENT_FILE` to that file's path. No proprietary game DLLs are included here. Dependency versions remain locked in the upstream `Cargo.lock`.

## Evidence and scope

[Validation](VALIDATION.md) separates native test results, standalone DLL loading and the actual MXL startup check. It does not claim a measured multiplayer performance improvement or compatibility with all MXL releases.

The integrated logic and its guards are described in [MXL-TIMING.md](MXL-TIMING.md). Original D2Client files stay unchanged on disk; the fork replaces `d2fps.dll` and applies its client corrections in memory. Normal upstream D2FPS hooks remain part of the rebuilt DLL.

## Credits and licenses

Based on Jarcho's D2FPS, with the MXL timing integration by Phroster. Original authorship and license files are retained.

- `d2fps`, `d2interface` and the integrated timing changes: [GPL-3.0](../LICENSE-GPL.txt).
- `bin_patch` and `bin_patch_mac`: [Apache-2.0](../LICENSE-APACHE.txt) or [MIT](../LICENSE-MIT.txt).

Each binary release includes a corresponding source archive and the license files.
