<p align="center">
  <img src="docs/banner-dx12.svg" alt="MXL Smooth Motion DX12" width="880">
</p>

**Does Median XL feel a little choppy online, even with high FPS?** This helps smooth out those small pauses in movement. It also includes D2GL's graphics options. The game stays at its normal speed.

Now with **DX12**, the same **Ctrl+O menu**, and **ReShade support**.

### [Download MXL Smooth Motion DX12 1.0](https://github.com/Phroster/mxl-smooth-motion-dx12/releases/download/v1.0/mxl-smooth-motion-dx12-1.0.zip)

## Install — copy, paste, play

1. **Open the Median XL launcher.** Let it finish updating. Choose **Glide or DirectDraw** and turn **Windowed** off. Under **Unofficial Graphic Drivers**, tick both **Glide3x.dll** and **Ddraw.dll**, like this:

   ![Tick Glide3x.dll and Ddraw.dll under Unofficial Graphic Drivers](docs/launcher-settings.png)

2. **Close the game and launcher.** Download the ZIP above and extract it.
3. **Copy these five files** into your Median XL game folder, next to `Game.exe`. Choose **Replace**:

   ```text
   glide3x.dll
   ddraw.dll
   d2gl.mpq
   d2gl.ini
   d2fps.ini
   ```

4. **Start the game and play.** The smoothing fix turns on by itself.

**That's it. You don't need to edit any settings or run an installer.**

Leave the existing **`d2fps.dll`** alone — the game still needs it.

Already changed your graphics settings? Back up your files first. The included INIs replace those settings with ours. [How to keep your settings](docs/INSTALL.md#keep-your-existing-settings).

## Check it or tweak it

- **Check it's working:** press **Ctrl+O**. Look for **Multiplayer Smoothing Fix: On**.
- **Change the picture:** use the same Ctrl+O menu.
- **Play in a window:** press **Alt+Enter** after starting. Keep Windowed off in the launcher.
- **Change the FPS limit:** open the **FPS tab** in Ctrl+O. Save, then restart the game. By default, it follows your monitor.

You can still edit `d2gl.ini` and `d2fps.ini` in Notepad if you prefer.

[More settings and help](docs/SETTINGS.md)

## ReShade

Choose **DirectX 10/11/12** in the [ReShade installer](https://reshade.me) for `Game.exe`. If you used ReShade with OpenGL before, switch it to DirectX. Keep your preset and shader folder. ReShade is optional and is not included.

## Which game is this for?

**Median XL 2.14.0 with Diablo II: Lord of Destruction 1.13c, on Windows 10 or newer.** Your graphics card must support **DX12**. Future game updates may need an updated fix.

If the status says **Unavailable**, [ask for help here](https://github.com/Phroster/mxl-smooth-motion-dx12/issues).

## Credits

Built on work by **Bayaraa, Pooquer, GavinK88 and Jarcho**, with the smoothing fix by **Phroster**. This is a community release.

[How it works](docs/ARCHITECTURE.md) · [Source/build guide](docs/BUILD.md) · [Checks performed](docs/VALIDATION.md) · [Original projects](docs/UPSTREAM.json) · [License](LICENSE)
