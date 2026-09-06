# Make it feel and look right

The five-file installation already supplies working settings. Everything on this page is optional. **Close the game before editing an INI**, save it, then start the game again.

## FPS and movement — d2fps.ini

| Setting | What it does |
|---|---|
| `fps=0` | Follows your monitor's refresh rate. This is the default. |
| `fps=120` | Example of a fixed 120 FPS target. Replace 120 with your preferred number. |
| `bg-fps=25` | Uses 25 FPS when the game is in the background. |
| `menu-fps=true` | Enables higher frame rates in menus. |
| `game-fps=true` | Enables higher frame rates while playing. |
| `motion-smoothing=true` | Smooths movement between the game's updates. Keep this on for the intended result. |
| `arcane-bg=false` | Leaves an optional background change off because it does not match this MXL setup. |

Edit the existing line for each setting; do not keep both `fps=0` and `fps=120`. Leave `integrity-checks=true` and `reapply-patches=true`.

The FPS value is a target: your display settings and hardware can still limit the actual rate. The multiplayer fix does not require a particular FPS number.

## Picture and window settings — Ctrl+O

Use the normal **Ctrl+O** graphics menu to adjust shaders, sharpening, bloom, HD text, cursor and window settings. These are also saved in `d2gl.ini`.

- **Too soft or too sharp?** Adjust the upscale shader and sharpening.
- **Too bright or glowing?** Adjust bloom or switch it off.
- **Prefer a window?** Change fullscreen in the menu or use Alt+Enter after startup. Leave the launcher's Windowed option off and remove `-w` from custom shortcuts.
- **Want V-Sync?** Use the menu's V-Sync option. It stays available.

Existing visual settings are kept by the installer. If you use the supplied template, it starts with xBRZ scaling, HD text/cursor, sharpening, FXAA and bloom. Adjust these to your taste.

## Keep the frame controls in one place

D2FPS manages the FPS limit and movement smoothing. This build automatically disables the overlapping D2GL controls after D2FPS initializes.

The supplied `d2gl.ini` also starts with:

```ini
[Screen]
foreground_fps=false
background_fps=false

[Feature]
motion_prediction=false

[Other]
frame_latency=1
```

These are excerpts from existing sections. Edit the values in your file rather than adding duplicate sections. `frame_latency=1` keeps the render queue short.

## The multiplayer fix

**Multiplayer Smoothing Fix: On** means the built-in timing correction activated. It is automatic and has no toggle. It keeps online movement going through small gaps between updates; it does not change the game's speed.

The original `motion-smoothing` setting in D2FPS still controls whether smoothed movement is drawn. Leave it enabled to use the intended setup.

## D2FPS loading

The package loads the official `d2fps.dll` automatically. If your `d2gl.ini` already has this line, you can keep it:

```ini
load_dlls_early=d2fps.dll:stdcall:_Init@0
```

The normal entry is recognized and is not initialized twice by D2GL. Keep other DLL entries you use. You do not need to add this line to a fresh installation.

## What is d2gl.mpq?

It holds the fonts, textures and shaders used by D2GL. Keep the included MPQ beside the renderer DLLs. It is the unchanged matching asset archive; you only need the menu and INIs to configure the game.

## Troubleshooting

| Problem | What to check |
|---|---|
| The game refuses `-w` or the new renderer does not activate | Select Glide or DirectDraw and turn Windowed off in the launcher. Remove `-w` from custom shortcuts; use Alt+Enter after startup. |
| The launcher replaces the renderer | Under **Unofficial Graphic Drivers**, tick both **Glide3x.dll** and **Ddraw.dll**, then copy the package files again after updates finish. |
| The launcher restores `d2fps.dll` | Keep it. The official file supplies the FPS engine; our D2GL applies the fix at startup. Restoring the supported official file is expected. |
| The fix says Unavailable | Confirm you used the folder with `Game.exe` and `D2Sigma.dll`. Read `mxl-smoothing.log`; newer game files may need an updated release. |
| Movement still looks unsmoothed | Check `game-fps=true` and `motion-smoothing=true` in `d2fps.ini`, then restart. |
| No new menu title or status | Check that the selected renderer DLL was copied into the actual game folder and was not replaced by the launcher. |
| Missing fonts or shaders | Copy the matching `d2gl.mpq` beside the DLLs. |
| FPS is lower after Alt-Tab | `bg-fps=25` applies while the game is in the background. |
| G-Sync or FreeSync behaves differently with another monitor | This package does not change driver settings. Display/driver behavior is separate from the movement correction. |

For help, [open an issue](https://github.com/Phroster/mxl-smooth-motion/issues) with the game version, Glide or DirectDraw mode, and the relevant smoothing log lines.

[Back to the player guide](../README.md)
