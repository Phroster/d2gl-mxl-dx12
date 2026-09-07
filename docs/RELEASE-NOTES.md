# MXL Smooth Motion DX12 1.0

**Helps Median XL feel smoother online, even when your FPS is already high.** Now with DX12 graphics and the familiar Ctrl+O menu.

### [Download mxl-smooth-motion-dx12-1.0.zip](https://github.com/Phroster/mxl-smooth-motion-dx12/releases/download/v1.0/mxl-smooth-motion-dx12-1.0.zip)

## Copy, paste, play

1. Open the Median XL launcher and let it finish updating. Choose **Glide or DirectDraw** and turn **Windowed** off. Under **Unofficial Graphic Drivers**, tick both **Glide3x.dll** and **Ddraw.dll**, like this:

   ![Tick Glide3x.dll and Ddraw.dll under Unofficial Graphic Drivers](https://raw.githubusercontent.com/Phroster/mxl-smooth-motion-dx12/v1.0/docs/launcher-settings.png)

2. Close the game and launcher. Download and extract the ZIP.
3. Copy these **five files** into your Median XL game folder, next to `Game.exe`. Choose **Replace**:

   ```text
   glide3x.dll
   ddraw.dll
   d2gl.mpq
   d2gl.ini
   d2fps.ini
   ```

4. **Start the game and play.** The smoothing fix turns on by itself.

**No settings to edit and no installer to run.** Leave the existing **`d2fps.dll`** alone — the game still needs it.

Already have custom settings? Back up your files first. Copying the INIs replaces them with ours. [Keep your settings instead](https://github.com/Phroster/mxl-smooth-motion-dx12/blob/main/docs/INSTALL.md#keep-your-existing-settings).

## Check it or tweak it

Press **Ctrl+O** and look for **Multiplayer Smoothing Fix: On**. That menu also lets you change the picture. Use its **FPS tab** to change your FPS limit; save and restart after making changes.

Want a window? Press **Alt+Enter** after starting. Keep Windowed off in the launcher.

**Using ReShade?** Choose **DirectX 10/11/12** in its installer. Switch an old OpenGL installation to DirectX and keep your preset. ReShade is optional and is not included.

[Full guide and help](https://github.com/Phroster/mxl-smooth-motion-dx12)

For **Median XL 2.14.0 / Diablo II 1.13c on Windows 10+**, with a graphics card that supports **DX12**.

Already using this DX12 version? You do not need to reinstall. The source ZIP is for developers.

Community release based on work by Bayaraa, Pooquer, GavinK88 and Jarcho. Smoothing fix by Phroster.
