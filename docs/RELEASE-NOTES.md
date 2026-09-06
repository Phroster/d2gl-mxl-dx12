# MXL Smooth Motion 1.0

**D2GL + D2FPS for Median XL, with smoother online movement.**

This community release brings the graphics wrapper and multiplayer smoothing fix together in one package. It is aimed at the tiny movement pauses that can make online play feel less smooth than single player, even with high FPS.

## Download

**Choose [mxl-smooth-motion-1.0.zip](https://github.com/Phroster/mxl-smooth-motion/releases/download/v1.0/mxl-smooth-motion-1.0.zip)** for the game files and instructions.

The source ZIP is for developers. `SHA256SUMS.txt` contains the download checksums.

## What is included?

- D2GL for **Glide and DirectDraw**, with the multiplayer fix built in.
- **Automatic activation**, with **Multiplayer Smoothing Fix: On** shown in the Ctrl+O menu.
- Matching `d2gl.mpq` for fonts, textures and shaders.
- Editable `d2gl.ini` and `d2fps.ini`, plus a plain-language settings guide.
- Optional installer with backups and a restore tool.
- Complete corresponding source and original license notices.

The package uses the launcher's **official `d2fps.dll`** as its FPS engine. Keep that file even if the launcher restores it: our D2GL applies the fix at each startup. It is not included in the ZIP.

## Install

1. Let the Median XL launcher finish updating. Enable **Unofficial Graphics Drivers** for **Glide** and **DirectDraw**.
2. Close the game and launcher. Back up your current renderer DLLs, MPQ and INIs.
3. Extract the ZIP. Copy **`glide3x.dll`, `ddraw.dll` and `d2gl.mpq`** into the actual game folder containing `Game.exe` and `D2Sigma.dll`, choosing **Replace**.
4. Keep your own INIs; copy a supplied INI only if yours is missing. In `d2fps.ini`, use:

```ini
fps=0
bg-fps=25
menu-fps=true
game-fps=true
motion-smoothing=true
arcane-bg=false
```

5. Launch normally. Press **Ctrl+O** and check **Multiplayer Smoothing Fix: On**.

`fps=0` follows your monitor refresh rate. Change it to a number if you prefer a fixed FPS target. Graphics settings remain available in Ctrl+O.

D2FPS loads automatically. An existing `d2gl.ini` entry can stay:

```ini
load_dlls_early=d2fps.dll:stdcall:_Init@0
```

**Upgrading from the private combined preview?** Replace the two renderer DLLs and keep your matching MPQ and INIs.

## Compatibility and help

For **Windows 10+ and the supported Median XL / Diablo II 1.13c files from MXL 2.14.0**. The game stays at its normal speed. This targets visual movement pauses, not connection lag, server delays or G-Sync behavior. Future game updates may need an updated release.

[Player guide](https://github.com/Phroster/mxl-smooth-motion#install-in-six-steps) · [Settings](https://github.com/Phroster/mxl-smooth-motion/blob/codex/combined/docs/SETTINGS.md) · [Report a problem](https://github.com/Phroster/mxl-smooth-motion/issues)

Based on D2GL by Bayaraa, Median XL adaptations by Pooquer/GavinK88, and D2FPS by Jarcho. Smoothing integration and packaging by Phroster. Independent community release.
