# MXL Smooth Motion DX12 1.1

**Smoother gameplay and sharper visuals for Median XL, with DX12 graphics and ReShade support.**

## What's new

- **Smoother crowded fights.** Fixes extra graphics work that was causing big FPS drops in busy areas.
- **Automatic map reveal.** Each act reveals when you enter it. A short pause can still happen on entry, but pressing T afterward no longer repeats the long reveal.
- **Performance recording is off by default.** It is still included if needed to investigate a problem. All gameplay fixes stay on.

### [Download mxl-smooth-motion-dx12-1.1.zip](https://github.com/Phroster/mxl-smooth-motion-dx12/releases/download/v1.1/mxl-smooth-motion-dx12-1.1.zip)

## Copy, paste, play

1. Open the Median XL launcher and let it finish updating. Choose **Glide or DirectDraw** and turn **Windowed** off. Under **Unofficial Graphic Drivers**, tick both **Glide3x.dll** and **Ddraw.dll**, like this:

   ![Tick Glide3x.dll and Ddraw.dll under Unofficial Graphic Drivers](https://raw.githubusercontent.com/Phroster/mxl-smooth-motion-dx12/v1.1/docs/launcher-settings.png)

2. Close the game and launcher. Download and extract the ZIP.
3. Copy these **six files** into your Median XL game folder, next to `Game.exe`. Choose **Replace**:

   ```text
   glide3x.dll
   ddraw.dll
   d2gl.mpq
   d2gl.ini
   d2fps.ini
   mxl-diagnostics.ini
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

**Upgrading from 1.0 or a private beta?** Close the game and launcher, then replace `glide3x.dll`, `ddraw.dll` and `mxl-diagnostics.ini`. Keep your existing graphics/FPS INIs and matching MPQ. The source ZIP is for developers.

[How to turn performance recording on when needed](https://github.com/Phroster/mxl-smooth-motion-dx12/blob/v1.1/docs/SETTINGS.md#optional-performance-recording)

Community release based on work by Bayaraa, Pooquer, GavinK88 and Jarcho. Smoothing fix by Phroster.
