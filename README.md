# D2GL + D2FPS for Median XL

Private combined project for smoother multiplayer movement, with normal editable INI files and the existing D2GL graphics menu.

**Multiplayer Smoothing Fix is built in and enabled automatically. There is no switch.** The D2GL menu shows its actual status. If the supported-file checks fail, it reports **Unavailable** and records the reason in `mxl-smoothing.log`.

## What gets installed

| File | Purpose |
|---|---|
| `glide3x.dll` | D2GL for Glide mode, with the built-in smoothing correction |
| `ddraw.dll` | D2GL for DirectDraw mode, with the same correction |
| `d2gl.mpq` | D2GL's fonts, textures and shaders; keep this beside the DLLs |
| `d2gl.ini` | Your editable graphics and window settings |
| `d2fps.ini` | Your editable FPS and movement settings |

The normal **official `d2fps.dll` remains in the game folder**. The installer obtains the supported official build if needed and verifies it. D2GL loads that engine, then applies the timing correction while the game runs. The launcher can continue checking the official file normally.

Both source trees are included here. The launcher-compatible package uses the official D2FPS engine with the integrated correction; the source D2FPS fork is also retained for development and comparison. It is not loaded as a second FPS engine.

## Install

1. In the Median XL launcher settings, enable **Unofficial Graphics Drivers** for the renderer you use (`glide3x.dll` or `ddraw.dll`). Enable both if you want to switch between them. Close the game and launcher.
2. Extract the combined ZIP into a separate folder.
3. Run its installer from PowerShell, supplying your actual game folder:

   ```powershell
   .\scripts\install.ps1 -GameDirectory 'G:\Median XL\median-xl'
   ```

4. Start the game normally. For Glide, use the launcher's Glide/fullscreen settings or your usual `-3dfx` shortcut without `-w`; Alt+Enter still changes window mode in game. Open the D2GL menu with **Ctrl+O** to see the smoothing status.

The installer backs up everything it replaces. It preserves existing visual settings and applies the pacing defaults described below. To keep your existing foreground FPS target, add `-PreserveFrameRate` to the install command. An optional `-OfficialD2FpsPath` accepts a local copy of the supported official DLL; its hash is checked too.

For manual installation, use the official D2FPS already supplied by the launcher, copy the two renderer DLLs and `d2gl.mpq`, and use the supplied INIs as templates. Do not overwrite your own visual settings without a backup.

## Editable defaults

- D2FPS follows the monitor refresh rate (`fps=0`) and limits background rendering to 25 FPS.
- D2FPS is the single FPS limiter and movement smoother. D2GL's duplicate limiter/prediction options stay off.
- Frame latency stays at 1. The default templates preserve the tested HD text and visual profile.
- D2FPS's optional Arcane background patch is disabled because it does not match the tested MXL client; it was already being disabled automatically in the previous setup.
- Existing shader, bloom, sharpening and other appearance choices remain editable. See [settings](docs/SETTINGS.md).

These are conservative defaults, not a claimed percentage performance gain. No NVIDIA profile or Windows display setting is changed.

## Restore the previous setup

Close the game and launcher, then run `restore.ps1` from the backup directory printed by the installer. Later INI edits are preserved in that backup directory before the original configuration is restored. If another program has changed a DLL since installation, the restore script stops for version review.

## Build

Install Visual Studio 2022 C++ Build Tools and a Windows SDK, then run `./build.ps1`. It runs native smoothing tests and builds both renderer DLLs without deploying them to a game.

Run `python scripts/package.py` to create the ZIP under `dist/`. The D2FPS workspace can also be built separately with `d2fps/build-mxl.ps1`; that is an optional development build, not the runtime DLL shipped by this package.

[Architecture and compatibility](docs/ARCHITECTURE.md) · [Build/test evidence](docs/VALIDATION.md) · [Source versions](docs/UPSTREAM.json)

Based on D2GL by Bayaraa, the MXL work by Pooquer/GavinK88, and D2FPS by Jarcho, with the smoothing integration by Phroster. Original licenses and third-party notices remain in each source tree and the package.
