# D2FPS — Median XL Multiplayer Smoothing

A small update aimed at reducing the tiny movement stutters that can make online play feel less smooth than single player, even when your FPS is high.

**[Download 1.0 — DLL only](https://github.com/Phroster/d2fps-mxl-smoothing/releases/download/v1.0/d2fps.dll)** · **[Download ZIP with instructions](https://github.com/Phroster/d2fps-mxl-smoothing/releases/download/v1.0/d2fps-mxl-smoothing-1.0.zip)**

## What does it do?

Diablo II updates movement in steps. D2FPS draws the movement between those steps. This version uses more precise timing and lets that movement continue briefly when the next update arrives a little late. This can reduce the small pauses you notice while moving online.

Your existing FPS and graphics settings are kept. The game still plays at its normal speed.

## Who is it for?

Median XL players on **Windows 10 or newer**, using **Diablo II 1.13c with D2GL and D2FPS already installed**. Look for an existing `d2fps.dll` in your game folder.

Support is limited to the game version described in the [compatibility details](docs/DEVELOPER-GUIDE.md#supported-game). A future game update may need an updated fix too. This is a community-made update, not an official Median XL release.

## Install

1. **Close the game.**
2. Find your Median XL game folder — the one containing `Game.exe` and `d2fps.dll`. Make a backup of your current `d2fps.dll` somewhere outside that folder.
3. Download the new **`d2fps.dll`** above, or extract it from the ZIP. Copy it into the game folder and choose **Replace**.
4. Launch the game normally.

**For an existing D2FPS setup, replacing that one file is all you need to do.** Keep your own `d2fps.ini` and graphics settings.

In **`d2gl.ini`**, make sure D2FPS is listed like this:

```ini
load_dlls_early=d2fps.dll:stdcall:_Init@0
```

If that line already lists other DLLs, keep those entries and include D2FPS only once.

## Check that it is working

After launching, open `d2fps.log` in the game folder and look for **`MXL timing ACTIVE`**.

If that line is missing, or you see **`MXL timing REFUSED`**, the extra fix has not activated. Check that you replaced the file in the correct game folder. If the launcher updated the game, it may have restored the original D2FPS or changed the supported game files. [Report a problem](https://github.com/Phroster/d2fps-mxl-smoothing/issues) with the relevant log lines and your game version.

## Go back to your old setup

Close the game and copy your backed-up `d2fps.dll` into the game folder again.

---

Based on [Jarcho's D2FPS](https://github.com/Jarcho/d2-rs), with the Median XL timing changes by Phroster.

[How the code works](docs/MXL-TIMING.md) · [Checks performed](docs/VALIDATION.md) · [Build and compatibility details](docs/DEVELOPER-GUIDE.md) · [GPL-3.0 license](LICENSE-GPL.txt)
