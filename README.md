<p align="center">
  <img src="docs/banner.svg" alt="MXL Smooth Motion — D2GL + D2FPS for Median XL" width="880">
</p>

<p align="center">
  <strong>Smoother online movement. Your familiar graphics menu.</strong><br>
  A community project combining D2GL, D2FPS and an automatic multiplayer smoothing fix.
</p>

<p align="center">
  <a href="https://github.com/Phroster/mxl-smooth-motion/releases/download/v1.0/mxl-smooth-motion-1.0.zip"><strong>Download 1.0</strong></a> ·
  <a href="docs/SETTINGS.md">Settings guide</a> ·
  <a href="https://github.com/Phroster/mxl-smooth-motion/issues">Get help</a>
</p>

## What does it do?

Does Median XL feel smooth in single player, but slightly choppy online even with a high FPS counter? **MXL Smooth Motion** is aimed at those small pauses in movement.

Diablo II updates movement in steps. D2FPS draws the movement between those steps. This fix uses more precise timing and allows movement to continue briefly when the next online update arrives a little late. The game still plays at its normal speed.

You get D2GL's graphics, D2FPS's higher frame rates and movement smoothing, and the multiplayer fix in one package. **The extra fix is on automatically**, with no switch to hunt for. It does not fix connection lag or server delays.

## Before you install

For **Windows 10 or newer**, with the supported **Median XL / Diablo II: Lord of Destruction 1.13c** setup. The supported files are from the Median XL **2.14.0** distribution. This is a community project for the classic game, not Diablo II: Resurrected.

Keep the launcher's official **`d2fps.dll`**. It still provides the FPS engine. The fix is built into this package's D2GL files and is applied each time the game starts, so the launcher restoring the supported official D2FPS does not undo the fix. Both projects' source code is included here.

## Install in six steps

1. **Let the Median XL launcher finish updating.** This also restores the official D2FPS if you used an older custom version.
2. In the launcher settings, enable **Unofficial Graphics Drivers** for **Glide** and **DirectDraw**. This lets you keep these custom graphics files.
3. **Close the game and launcher.** Find the actual game folder containing `Game.exe` and `D2Sigma.dll`. Back up its `glide3x.dll`, `ddraw.dll`, `d2gl.mpq`, `d2gl.ini` and `d2fps.ini` somewhere separate.
4. [Download **mxl-smooth-motion-1.0.zip**](https://github.com/Phroster/mxl-smooth-motion/releases/download/v1.0/mxl-smooth-motion-1.0.zip) and extract it. Copy **`glide3x.dll`, `ddraw.dll` and `d2gl.mpq`** into the game folder, choosing **Replace**.
5. **Keep your existing INI files.** Copy a supplied INI only if yours is missing. Open `d2fps.ini` in Notepad and set the values below; edit existing lines rather than adding duplicates.
6. Start the game normally. Press **Ctrl+O** and look for **Multiplayer Smoothing Fix: On**.

```ini
fps=0
bg-fps=25
menu-fps=true
game-fps=true
motion-smoothing=true
arcane-bg=false
```

`fps=0` follows your monitor's refresh rate. You can use a number instead, such as `fps=120`. See the [settings guide](docs/SETTINGS.md) for FPS, picture quality and troubleshooting.

**Already using the private combined build?** Replace the two renderer DLLs with this release and keep your matching MPQ and INIs.

<details>
<summary>Optional installer with automatic backup</summary>

Extract the release ZIP into a separate folder. With the game and launcher closed, open PowerShell there and run the following, replacing the example path with your game folder:

```powershell
.\scripts\install.ps1 -GameDirectory 'C:\Games\Median XL\median-xl'
```

It checks compatibility, keeps or obtains the supported official D2FPS, preserves your visual settings and applies the FPS defaults above. Add `-PreserveFrameRate` to keep your existing FPS target. It prints the backup folder and includes a restore script.

See [installation and restore details](docs/INSTALL.md).

</details>

## Configure it your way

| What you want to change | Where to change it |
|---|---|
| FPS target or FPS while Alt-Tabbed | `d2fps.ini` |
| Sharpening, bloom, shaders, HD text or fullscreen | **Ctrl+O**, or `d2gl.ini` |
| Check the multiplayer fix | **Ctrl+O → Multiplayer Smoothing Fix** |

D2FPS handles the frame limit and movement smoothing automatically. The extra D2GL limiters and motion prediction are disabled when D2FPS loads.

D2FPS also loads automatically. An existing line like this in `d2gl.ini` is supported and can stay:

```ini
load_dlls_early=d2fps.dll:stdcall:_Init@0
```

You do not need to add it for this build. Keep any other entries you already use.

## What is in the download?

| File | What it is for |
|---|---|
| `glide3x.dll` | D2GL for Glide, with the multiplayer fix |
| `ddraw.dll` | D2GL for DirectDraw, with the same fix |
| `d2gl.mpq` | The matching fonts, textures and shaders |
| `d2gl.ini` / `d2fps.ini` | Editable settings templates |
| `START-HERE.txt` / `docs/` | Installation and settings help |
| `scripts/` | Optional installer and restore tool |

**The MPQ stays beside the DLLs.** You do not need to edit it. The official `d2fps.dll` comes from the Median XL launcher, so it is not included in the ZIP.

## Need help or want to undo it?

If the status says **Unavailable**, check `mxl-smoothing.log` in the game folder. A game update can change the supported files. [Open an issue](https://github.com/Phroster/mxl-smooth-motion/issues) with your game version, rendering mode and the relevant log lines.

To undo a manual installation, close the game and launcher and restore your backed-up files. If you used the installer, follow the [restore guide](docs/INSTALL.md#restore-an-installer-backup).

## Credits and source

Built on **D2GL by Bayaraa**, the **Median XL adaptations by Pooquer and GavinK88**, and **D2FPS by Jarcho**. Multiplayer smoothing integration and packaging by **Phroster**. This is an independent community release.

[How the fix works](docs/ARCHITECTURE.md) · [Build from source](docs/BUILD.md) · [Validation](docs/VALIDATION.md) · [Upstream versions](docs/UPSTREAM.json) · [GPL-3.0 license](LICENSE)

Original project licenses, authorship and third-party notices are retained. The release includes the corresponding source archive.
