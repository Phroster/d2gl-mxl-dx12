# D2FPS — Median XL Multiplayer Smoothing 1.0

Helps reduce the tiny movement pauses that can make online play feel less smooth than single player. Your FPS and graphics settings are kept, and the game still plays at its normal speed.

## Install

For Median XL players already using D2GL and D2FPS:

1. Close the game and back up your current `d2fps.dll` outside the game folder.
2. Download **`d2fps.dll`** below, or take it from **`d2fps-mxl-smoothing-1.0.zip`**.
3. Replace the old `d2fps.dll` in your Median XL game folder.
4. Launch normally. Keep your existing settings.

In **`d2gl.ini`**, make sure D2FPS is listed like this:

```ini
load_dlls_early=d2fps.dll:stdcall:_Init@0
```

If that line already lists other DLLs, keep those entries and include D2FPS only once.

After launching, look for **`MXL timing ACTIVE`** in `d2fps.log`.

## Downloads

- **`d2fps.dll`** — the game file you need to replace.
- **`d2fps-mxl-smoothing-1.0.zip`** — the same file with instructions.
- **`d2fps-mxl-smoothing-1.0-source.zip`** — source code for developers.
- **`SHA256SUMS.txt`** — download checksums.

For Windows 10 or newer and the supported Median XL / Diablo II 1.13c setup. This is a community update, not an official Median XL release.

This is the cleaned 1.0 package for **[d2fps-mxl-smoothing](https://github.com/Phroster/d2fps-mxl-smoothing)**. It replaces the previous download under the same version. The timing calculations are unchanged; client-code conflicts are checked directly, and the version labels are aligned to 1.0. Its new DLL checksum is supplied below.

Seven native tests passed, covering 378 timing cases, and the rebuilt DLL passed a standalone loading check. The shared timing behavior was previously checked at startup in Median XL. No measured improvement percentage or broad game-version compatibility is claimed.

DLL SHA-256: `3b54d5b7c30c4001cfde7e5f8a183db041fb1806182709ddc92f790a7085791f`.

To undo the change, close the game and restore your backed-up file.
