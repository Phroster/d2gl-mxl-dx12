# MXL Smooth Motion DX12 1.0

**DX12 is now the main version.** Smoother movement, D2GL graphics options, the **Ctrl+O menu**, an **FPS settings tab**, and **ReShade support**.

### [Download mxl-smooth-motion-dx12-1.0.zip](https://github.com/Phroster/mxl-smooth-motion-dx12/releases/download/v1.0/mxl-smooth-motion-dx12-1.0.zip)

## Copy, paste, play

1. Let the Median XL launcher finish updating. Choose **Glide or DirectDraw**, turn **Windowed** off, and tick both **Glide3x.dll** and **Ddraw.dll** under **Unofficial Graphic Drivers**:

   ![Enable both custom graphics DLLs](https://raw.githubusercontent.com/Phroster/mxl-smooth-motion-dx12/v1.0/docs/launcher-settings.png)

2. Close the game and launcher. Download and extract the ZIP.
3. Copy these **five files** into your game folder, next to `Game.exe`, and choose **Replace**:

   ```text
   glide3x.dll
   ddraw.dll
   d2gl.mpq
   d2gl.ini
   d2fps.ini
   ```

4. **Start playing.** The smoothing fix turns on automatically.

Keep the official **`d2fps.dll`**. Back up custom settings before replacing the INIs.

## Make it yours

Press **Ctrl+O** for graphics and the new **FPS** tab. Restart after saving FPS changes. Use **Alt+Enter** for fullscreen/windowed mode.

**ReShade:** choose **DirectX 10/11/12** in its installer and keep your preset. Switch an existing OpenGL ReShade installation to DirectX as well. ReShade is optional and is not bundled.

[Full guide](https://github.com/Phroster/mxl-smooth-motion-dx12) · [Settings](https://github.com/Phroster/mxl-smooth-motion-dx12/blob/main/docs/SETTINGS.md) · [Help](https://github.com/Phroster/mxl-smooth-motion-dx12/issues)

For Windows 10+, a DX12-capable GPU, and the supported MXL 2.14.0 / Diablo II 1.13c files.

This replaces the earlier OpenGL download under **1.0**. It contains the exact accepted DX12 renderer binaries. If you already installed that DX12 build, you do not need to reinstall.

The source ZIP is for developers. Original credits and licenses are included.
