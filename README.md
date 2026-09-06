<p align="center">
  <img src="docs/banner.svg" alt="MXL Smooth Motion DX12" width="880">
</p>

**DirectX 12 graphics and smoother movement for Median XL.** Keeps the familiar **Ctrl+O menu**, adds easy FPS settings, and works with **ReShade for DirectX**. The multiplayer smoothing fix turns on by itself.

### [Download MXL Smooth Motion DX12 1.0](https://github.com/Phroster/mxl-smooth-motion-dx12/releases/download/v1.0/mxl-smooth-motion-dx12-1.0.zip)

## Install — copy, paste, play

1. **Open the Median XL launcher.** Let updates finish. Choose **Glide or DirectDraw** and turn **Windowed** off. Under **Unofficial Graphic Drivers**, tick both **Glide3x.dll** and **Ddraw.dll**:

   ![Tick both custom graphics DLLs](docs/launcher-settings.png)

2. **Close the game and launcher.** Download the ZIP above and extract it.
3. **Copy these five files** into your Median XL game folder, next to `Game.exe`. Choose **Replace**:

   ```text
   glide3x.dll
   ddraw.dll
   d2gl.mpq
   d2gl.ini
   d2fps.ini
   ```

4. **Start the game and play.** Everything is already set up.

Both launcher choices use **DX12** with this mod. Leave the existing **`d2fps.dll`** alone — the game still needs it.

Back up your files first if you have custom settings. Copying the INIs replaces those settings with ours. [Keep your settings instead](docs/INSTALL.md#keep-your-existing-settings).

## Your settings are in Ctrl+O

- **Picture:** shaders, sharpening, bloom, HD text and cursor.
- **FPS tab:** change your FPS limit and movement settings. Save, then restart the game.
- **Smoothing status:** look for **Multiplayer Smoothing Fix: On**.
- **Windowed/fullscreen:** press **Alt+Enter** after starting.

The default FPS target follows your monitor. The INI files are still available if you prefer editing them.

## ReShade works too

Use [ReShade](https://reshade.me) with **DirectX 10/11/12** selected for `Game.exe`. Keep your existing preset and shader folder. If you previously used ReShade for OpenGL, switch that installation to DirectX too. ReShade is optional and is not included in this download.

[Settings and help](docs/SETTINGS.md) · [Report a problem](https://github.com/Phroster/mxl-smooth-motion-dx12/issues)

For **Windows 10+ with a DX12-capable GPU**, using the supported **Median XL 2.14.0 / Diablo II: Lord of Destruction 1.13c** files. The game stays at its normal speed.

## Credits

Built on work by **Bayaraa, Pooquer, GavinK88 and Jarcho**, with DX12 integration and the smoothing fix by **Phroster**. Community release.

[How it works](docs/ARCHITECTURE.md) · [Build from source](docs/BUILD.md) · [Checks performed](docs/VALIDATION.md) · [License](LICENSE)
