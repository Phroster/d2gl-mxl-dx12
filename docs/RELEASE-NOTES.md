# MXL Smooth Motion 1.0

**Helps Median XL feel smoother online, even when your FPS is already high.**

### [Download mxl-smooth-motion-1.0.zip](https://github.com/Phroster/mxl-smooth-motion/releases/download/v1.0/mxl-smooth-motion-1.0.zip)

## Copy, paste, play

1. Open the Median XL launcher and let it finish updating. Choose **Glide or DirectDraw** and turn **Windowed** off. Under **Unofficial Graphic Drivers**, tick both **Glide3x.dll** and **Ddraw.dll**, like this:

   ![Tick Glide3x.dll and Ddraw.dll under Unofficial Graphic Drivers](https://raw.githubusercontent.com/Phroster/mxl-smooth-motion/v1.0/docs/launcher-settings.png)

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

Already have custom settings? Back up your files first. Copying the INIs replaces them with ours. [Keep your settings instead](https://github.com/Phroster/mxl-smooth-motion/blob/codex/combined/docs/INSTALL.md#keep-your-existing-settings).

## Check it or tweak it

Press **Ctrl+O** and look for **Multiplayer Smoothing Fix: On**. That menu also lets you change the picture.

Want a window? Press **Alt+Enter** after starting. Keep Windowed off in the launcher.

[Full guide and help](https://github.com/Phroster/mxl-smooth-motion)

For **Median XL 2.14.0 / Diablo II 1.13c on Windows 10+**.

Still version **1.0**. Only the instructions changed; existing 1.0 users do not need to reinstall. The source ZIP below is for developers.

Community release based on work by Bayaraa, Pooquer, GavinK88 and Jarcho. Smoothing fix by Phroster.
