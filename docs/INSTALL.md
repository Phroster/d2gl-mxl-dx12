# Installation and restoring a backup

The [player guide](../README.md#install-in-six-steps) covers copying the files manually. This page explains the optional installer.

## Optional installer

1. Let the Median XL launcher update the game. Enable Unofficial Graphics Drivers for Glide and DirectDraw.
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

Launch normally and press **Ctrl+O**. Look for **Multiplayer Smoothing Fix: On**.

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
