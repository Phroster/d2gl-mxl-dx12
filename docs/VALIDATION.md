# Validation

Both Release/Win32 renderer targets build with Visual Studio 2022 Build Tools and the Windows SDK. The native embedded timing test executes five clock instruction forms, verifies refusal without partial changes, checks rollback after a simulated page-protection failure, and covers 168 clamp arithmetic/game-mode/register-preservation cases.

The included Rust D2FPS source workspace also compiled in its new location and passed all seven release-mode tests, covering 378 timing cases and executable client patch/rollback cases. This validates that source tree as an optional development build; it is not a second runtime engine in the package.

The installer and restore script were executed against an isolated fixture, not the user's game. Checks confirmed official D2FPS restoration, recommended pacing values, preserved custom visuals, optional foreground-FPS preservation, byte-exact rollback, preservation of later INI edits, refusal of an unsupported D2FPS input before game writes, and refusal to overwrite an externally changed binary during rollback.

Both wrappers are x86. Glide's 54 exported names/ordinals match the previously working GavinK88 renderer. DirectDraw exposes the imported fork's expected four entry points and provides the game's required `DirectDrawCreate`. Its optional vendor interface differs from the official MXL DirectDraw wrapper. The public 1.0 rebuild retains the combined preview's export names, ordinals and imports.

The multiplayer correction is included directly in each renderer. No separate runtime correction DLL is produced. The menu status is read-only and reports On only after the patch application/readback succeeds.

The MPQ is unchanged from the matching source and tested asset archive. Default templates retain the tested visual settings and adjust only the documented FPS/compatibility choices. No numerical FPS or frametime improvement is claimed for these default settings.

The current launcher's manifest and inspected exception logic were compared with the package: custom Glide/DirectDraw are allowed when their existing settings are enabled; the supplied MPQ matches the official SHA-1; the installer keeps/restores the matching official D2FPS; the INIs are not managed by that manifest. The official D2FPS download endpoint was also tested and its downloaded file matched the exact SHA-256 guard. This is static/configuration evidence, separate from an end-to-end launcher run.

Concise records: [embedded tests](NATIVE-TESTS.txt), [D2FPS source tests](D2FPS-SOURCE-TESTS.txt), [installer tests](INSTALLER-TESTS.txt), and [binary/manifest checks](BINARY-VERIFICATION.json). Build warnings were in inherited renderer code and missing vendor-library PDBs; both release builds completed.

The combined implementation was installed in the supported Median XL game folder and started successfully. Its startup log on 2026-09-06 confirms all six timing regions were applied and read back, with the simulation interval still 40 ms. D2FPS's log confirms the official engine, motion smoothing and automatic monitor-refresh target were active. See the [redacted runtime record](RUNTIME-CHECK.txt).

Public release 1.0 rebuilds that implementation with the MXL Smooth Motion menu, log and Windows product labels. Timing calculations and patch guards are unchanged. Absolute build-machine paths were removed from the renderer PDB references. The native timing tests passed again after rebuilding.

Evidence scope: successful guarded startup of the pre-branding combined implementation, native tests and package/installer checks. This is not a measured FPS/frametime comparison or a claim that every renderer, display configuration and future launcher update has been tested.

Detailed build logs and executable test results remain under the ignored `build/` directory. Only concise verification summaries are included in source/release documentation; original game DLLs and personal paths are not published.
