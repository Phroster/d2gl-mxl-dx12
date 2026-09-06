# MXL Smooth Motion architecture

The repository contains the full MXL D2GL source under `d2gl/`, the D2FPS source workspace under `d2fps/`, an integrated C timing module compiled into both D2GL renderer DLLs, editable config templates, build/package scripts and a reversible installer.

## Runtime

Only the renderer selected by the existing D2GL startup logic initializes. It reads the INI, loads the official D2FPS through its normal `_Init@0` entry, loads additional user DLLs, and applies the guarded timing correction. Duplicate standard D2FPS initializer entries are skipped by the D2GL loader after successful initialization. Sigma's later initializer call retains D2FPS's normal initialization guard.

The source D2FPS fork is included as maintained source/reference and an optional standalone development build. The launcher-compatible package does not statically link or run a second copy of that engine. The deployed engine is the official D2FPS file, with the corrections applied in process by D2GL. This is what preserves the launcher's expected on-disk D2FPS checksum.

The integrated correction replaces five clock operands and the 12-byte upper interpolation clamp: four D2Client clock references, D2FPS's non-SP clock reference, and its realm-only clamp. It retains the nominal 40 ms simulation interval, the original negative-time path, SP/LAN clamp behavior and post-draw restoration of authoritative positions.

## Exact supported files

| File | SHA-256 |
|---|---|
| D2Client.dll | `dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906` |
| Official d2fps.dll | `db9de4d4d320a7b70e66fe6b4aaa0e6f1560a5300a4993cc81cf4512ab1240c1` |

D2Sigma must be loaded. PE identity, all six original instruction regions, the existing precise-clock imports and the 40 ms interval are validated before writes. A failed preflight leaves the patch sites unchanged. Writes are read back and instruction-cache/page-protection handling is retained. The menu reports On only after successful application; otherwise it reports Unavailable and the startup log explains why.

No launcher code, update manifest, graphics-driver setting or global Windows timer configuration is altered by the new timing module. D2GL's existing multimedia timer request remains upstream behavior.

## Launcher compatibility

The inspected Median XL launcher allows custom `glide3x.dll` and `ddraw.dll` when both checkboxes under Unofficial Graphic Drivers are enabled. It continues checking the ordinary D2FPS file against the official SHA-1. The installer preserves/restores that exact official file instead of trying to make a modified D2FPS pass as the original.

Official D2FPS may be downloaded directly from the existing MXL 2.14.0 distribution endpoint during installation. It is checked against the exact SHA-256 before any game files are replaced. It is not redistributed inside this repository or package.

The INIs are not managed by the inspected release manifest. `d2gl.mpq` is managed and the bundled archive matches its official SHA-1 exactly. The launcher settings should be selected through the launcher's normal interface. Unknown game/D2FPS updates cause the integrated correction to refuse activation until reviewed.

## Packaging and ownership

The installer changes six named game files, backs up each existing file, stages every replacement first and checks the results. It preserves appearance values while applying a small set of pacing/compatibility defaults. A local-copy option avoids the network when the official D2FPS file is already available. Rollback checks binary identities and preserves later INI edits before restoring originals.

Game binaries, save files and runtime logs are excluded from source distribution. D2GL's existing MPQ and vendored build dependencies are retained with their upstream license notices.
