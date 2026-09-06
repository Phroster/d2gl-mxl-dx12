<p align="center">
  <img src="docs/banner.svg" alt="MXL Smooth Motion — D2GL + D2FPS for Median XL" width="880">
</p>

**Does Median XL feel a little choppy online, even with high FPS?** This helps smooth out those small pauses in movement. It also includes D2GL's graphics options. The game stays at its normal speed.

### [Download MXL Smooth Motion 1.0](https://github.com/Phroster/mxl-smooth-motion/releases/download/v1.0/mxl-smooth-motion-1.0.zip)

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
- **Change the FPS limit:** open `d2fps.ini` in Notepad. `fps=0` follows your monitor; `fps=120` sets a 120 FPS target.

[More settings and help](docs/SETTINGS.md)

## Which game is this for?

**Median XL 2.14.0 with Diablo II: Lord of Destruction 1.13c, on Windows 10 or newer.** Future game updates may need an updated fix.

If the status says **Unavailable**, [ask for help here](https://github.com/Phroster/mxl-smooth-motion/issues).

## Credits

Built on work by **Bayaraa, Pooquer, GavinK88 and Jarcho**, with the smoothing fix by **Phroster**. This is a community release.

[How it works](docs/ARCHITECTURE.md) · [Source/build guide](docs/BUILD.md) · [Checks performed](docs/VALIDATION.md) · [Original projects](docs/UPSTREAM.json) · [License](LICENSE)
