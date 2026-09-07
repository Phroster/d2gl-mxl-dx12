# Keeping your settings and restoring a backup

The [main guide](../README.md#install--copy-paste-play) is the normal installation: copy six files and play.

## Keep your existing settings

Back up your current files. After setting the launcher options in the main guide and closing the game and launcher, copy only **`glide3x.dll`, `ddraw.dll`, `d2gl.mpq` and `mxl-diagnostics.ini`**. Keep your own `d2gl.ini` and `d2fps.ini`.

Open **Ctrl+O → FPS** and make sure high FPS in game, high FPS in menus and smooth movement are enabled. Save any changes and restart. Keep your preferred FPS target.

The supplied `mxl-diagnostics.ini` turns performance recording off without disabling the fixes. If you have no graphics/FPS INIs, use the two included in the download.

Already using our DX12 1.0 or a private beta? Replace `glide3x.dll`, `ddraw.dll` and `mxl-diagnostics.ini`. Your matching MPQ and graphics/FPS settings can stay as they are.

## ReShade

Run the official [ReShade installer](https://reshade.me), select your game folder's `Game.exe`, and choose **DirectX 10/11/12**. Keep your existing preset and shader folder.

If ReShade was previously installed for OpenGL, switch that installation to DirectX. The verified setup uses ReShade 6.6.2.2082 as `dxgi.dll`. ReShade is not included in the mod download.

## Undo an installation

Close the game and launcher, then restore the files you backed up. If you changed ReShade's API as well, restore its previous installation too.

Older scripted-install backups include their own `restore.ps1`. Use the script inside the matching backup; it checks whether the files still match before restoring them.

Your saves are not part of this graphics package.
