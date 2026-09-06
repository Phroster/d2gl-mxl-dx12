# Installation and restoring a backup

The [player guide](../README.md#install--copy-paste-play) is the normal installation: copy five files and play. This page is only for keeping custom settings, using the optional backup installer, or restoring a backup.

## Keep your existing settings

If you already have a custom setup, back up your files before changing them.

After configuring the launcher as shown in the player guide, close the game and launcher. Copy only `glide3x.dll`, `ddraw.dll` and `d2gl.mpq`, and keep your existing INIs.

In `d2fps.ini`, make sure these existing lines are enabled:

```ini
menu-fps=true
game-fps=true
motion-smoothing=true
```

Keep your preferred `fps` value. Use `arcane-bg=false` for the supported MXL setup. The [settings guide](SETTINGS.md) explains the remaining options.

Alternatively, the optional installer below preserves visual preferences and applies the package's FPS defaults for you.

## Optional installer

1. Let the Median XL launcher update the game. Select Glide or DirectDraw, enable Unofficial Graphics Drivers for that mode, and turn Windowed off.
2. Close both the game and launcher.
3. Extract `mxl-smooth-motion-1.0.zip` into a separate folder.
4. Open PowerShell in that extracted folder.
5. Run the command below, replacing the example path with the folder containing your `Game.exe` and `D2Sigma.dll`:

```powershell
.\scripts\install.ps1 -GameDirectory 'C:\Games\Median XL\median-xl'
```

The installer backs up the files it replaces, keeps your visual preferences and applies the documented FPS defaults. It verifies the supported game files first. If necessary, it downloads the matching official D2FPS directly from Median XL and checks it before changing the game folder.

If Windows does not allow PowerShell scripts on your PC, use the manual installation steps in the player guide.

### Keep your current FPS target

```powershell
.\scripts\install.ps1 -GameDirectory 'C:\Games\Median XL\median-xl' -PreserveFrameRate
```

All other documented installer defaults still apply.

### Use an existing official D2FPS copy

The installer automatically uses the copy in the game folder if it matches. To supply a copy from somewhere else:

```powershell
.\scripts\install.ps1 -GameDirectory 'C:\Games\Median XL\median-xl' -OfficialD2FpsPath 'C:\Downloads\d2fps.dll'
```

Only the supported official file is accepted.

## After installation

Launch in Glide or DirectDraw without `-w` and press **Ctrl+O**. Look for **Multiplayer Smoothing Fix: On**. Use Alt+Enter afterward if you prefer a window.

Keep `d2gl.mpq` and the official `d2fps.dll` in the game folder. Use [the settings guide](SETTINGS.md) to adjust your FPS and picture.

## Restore an installer backup

1. Close the game and launcher.
2. Open the backup folder printed by the installer, under `mxl-smooth-motion-backups` in the game folder.
3. Open PowerShell in that backup folder and run:

```powershell
.\restore.ps1
```

Later INI edits are saved in the backup folder before the original settings are restored. If a DLL was replaced again after installation, the restore tool stops so it does not overwrite an unrelated update.

Backups made by the earlier private package remain under `mxl-combined-backups`. Use the restore script inside that particular backup.

For a manual installation, restore the copies you made before installing.
