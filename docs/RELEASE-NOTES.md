# 🎮 MXL Smooth Motion DX12 1.0

**Smoother movement, steadier battles and sharper visuals for Median XL.**

### [⬇️ Download & play](https://github.com/Phroster/d2gl-mxl-dx12/releases/download/v1.0/mxl-smooth-motion-dx12-1.0.zip)

## ✨ Included

- **Smooth gameplay above 25 FPS** in single player and online.
- **Improved performance in crowded fights** with lots of enemies and effects.
- **DirectX 12 graphics** with HD text, HD cursor, upscaling, sharpening and bloom.
- **Automatic act map reveal** when you enter an act.
- **ReShade support** for DirectX presets.

Your FPS target follows your monitor by default. Game speed stays the same.

## 📦 Copy, paste, play

1. **Open the Median XL launcher** and let updates finish. Choose **Glide or DirectDraw**, turn **Windowed** off, and enable both **Glide3x.dll** and **Ddraw.dll** under **Unofficial Graphic Drivers**.

   ![Enable Glide3x.dll and Ddraw.dll in the launcher](https://raw.githubusercontent.com/Phroster/d2gl-mxl-dx12/v1.0/docs/launcher-settings.png)

2. **Close the game and launcher.** Download and extract the ZIP above.

3. **Copy these six files** into your Median XL game folder, beside `Game.exe`. Choose **Replace**.

   ```text
   glide3x.dll
   ddraw.dll
   d2gl.mpq
   d2gl.ini
   d2fps.ini
   mxl-diagnostics.ini
   ```

4. **Start playing.** Movement smoothing and automatic map reveal turn on by themselves.

**Keep the existing `d2fps.dll`.** The game still needs it. Back up your current files before replacing custom settings.

The ZIP contains only the six installation files and a single `LICENSES.txt` notice. All guides, screenshots and developer tools are available in the repository.

### ⬆️ Upgrading an existing installation?

Replace **`glide3x.dll`, `ddraw.dll` and `mxl-diagnostics.ini`**. Keep your graphics/FPS INIs and matching MPQ to preserve your setup.

## ⚙️ Your settings

- **Ctrl+O:** graphics options and smoothing status.
- **Ctrl+O → FPS:** FPS limit and movement settings. Save and restart after changes.
- **Alt+Enter:** switch fullscreen and windowed mode.
- **ReShade:** select `Game.exe` and **DirectX 10/11/12** in its installer. ReShade is downloaded separately.

Preparing a new act's map can briefly pause the game on entry. Pressing **T** afterward does not repeat the full reveal.

**Requires:** Median XL 2.14.0, Diablo II: Lord of Destruction 1.13c, Windows 10+ and a DirectX 12 graphics card.

## 💛 Credits & thanks

- **[Bayaraa](https://github.com/bayaraa/d2gl)** — D2GL's graphics, shaders, HD text/cursor and in-game menu.
- **[Jarcho](https://github.com/Jarcho/d2-rs/tree/main/d2fps)** — D2FPS, higher frame rates and movement smoothing.
- **Pooquer** — early Median XL adaptations of D2GL.
- **[GavinK88](https://github.com/GavinK88/d2gl-mxl-1.0)** — the D2GL fork for Median XL.
- **Median XL team** — the mod, its features and its community.
- **Phroster** — MXL Smooth Motion DX12 and maintenance of this edition.

Special thanks to the original projects' contributors, Blizzard North and the Diablo II modding community.

[📖 Player guide](https://github.com/Phroster/d2gl-mxl-dx12#readme) · [⚙️ Settings](https://github.com/Phroster/d2gl-mxl-dx12/blob/master/docs/SETTINGS.md)

[Source for this release](https://github.com/Phroster/d2gl-mxl-dx12/tree/v1.0) · [Source download](https://github.com/Phroster/d2gl-mxl-dx12/archive/refs/tags/v1.0.zip) · [Licensing & credits](https://github.com/Phroster/d2gl-mxl-dx12/blob/v1.0/docs/LICENSING.md)
