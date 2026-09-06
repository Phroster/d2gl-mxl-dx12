# Validation

Both Release/Win32 renderer targets build with Visual Studio 2022 Build Tools and the Windows SDK. The native embedded timing test executes five clock instruction forms, verifies refusal without partial changes, checks rollback after a simulated page-protection failure, and covers 168 clamp arithmetic/game-mode/register-preservation cases.

The included Rust D2FPS source workspace also compiled in its new location and passed all seven release-mode tests, covering 378 timing cases and executable client patch/rollback cases. This validates that source tree as an optional development build; it is not a second runtime engine in the package.

The installer and restore script were executed against an isolated fixture, not the user's game. Checks confirmed official D2FPS restoration, recommended pacing values, preserved custom visuals, optional foreground-FPS preservation, byte-exact rollback, preservation of later INI edits, refusal of an unsupported D2FPS input before game writes, and refusal to overwrite an externally changed binary during rollback.

Both wrappers are x86. Glide's 54 exported names/ordinals match the user's working GavinK88 renderer. DirectDraw exposes the imported fork's expected four entry points and provides the game's required `DirectDrawCreate`. Its optional vendor interface differs from the currently installed MXL DirectDraw wrapper; it is not claimed to be the same binary/API implementation. Runtime DirectDraw validation remains part of the installation test.

The multiplayer correction is included directly in each renderer. No separate runtime correction DLL is produced. The menu status is read-only and reports On only after the patch application/readback succeeds.

The MPQ is unchanged from the matching source and tested asset archive. Default templates retain the tested visual settings and adjust only the documented FPS/compatibility choices. No numerical FPS or frametime improvement is claimed for these default settings.

The current launcher's manifest and inspected exception logic were compared with the package: custom Glide/DirectDraw are allowed when their existing settings are enabled; the supplied MPQ matches the official SHA-1; the installer keeps/restores the matching official D2FPS; the INIs are not managed by that manifest. The official D2FPS download endpoint was also tested and its downloaded file matched the exact SHA-256 guard. This is static/configuration evidence, separate from an end-to-end launcher run.

Concise records: [embedded tests](NATIVE-TESTS.txt), [D2FPS source tests](D2FPS-SOURCE-TESTS.txt), [installer tests](INSTALLER-TESTS.txt), and [binary/manifest checks](BINARY-VERIFICATION.json). Build warnings were in inherited renderer code and missing vendor-library PDBs; both release builds completed.

This build has not been installed into the user's game during preparation. End-to-end game startup, both rendering modes and launcher update behavior must be assessed during the installation test. Build/static checks alone are not evidence of in-game activation.

Detailed build logs and executable test results remain under the ignored `build/` directory. Only concise verification summaries are included in source/release documentation; original game DLLs and personal paths are not published.
