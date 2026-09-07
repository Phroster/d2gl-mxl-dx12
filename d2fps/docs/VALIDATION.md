# D2FPS for Median XL 1.0 validation

The native tests and standalone loading check below apply to the clean 1.0 build. The separate MXL startup record covers a prior build with the same timing calculations.

## Built artifact

- Architecture: x86 / PE32.
- Rust: 1.94.1, `i686-pc-windows-msvc`, locked dependency versions, optimized release build with static CRT.
- Build recipe: `build-mxl.ps1`; local source/dependency paths are remapped in diagnostics.
- File version: `1.0.0.0`; startup label: `D2FPS MXL 1.0`.
- DLL size: 387072 bytes.
- SHA-256: `3b54d5b7c30c4001cfde7e5f8a183db041fb1806182709ddc92f790a7085791f`.
- Exports match the original installed D2FPS: `_DllMain@12`, `_Init@0`, `_Init@4`.
- This is a self-contained, unsigned community build of D2FPS.

## Native tests

All **7 tests passed in the optimized x86 release configuration**:

1. Clock-domain selection keeps the legacy client clock until the producer correction activates, and preserves SP's separate clock domain.
2. **378 timing cases** cover active/inactive correction, SP and realm/TCP/open-Battle.net modes, negative elapsed time, exact endpoints, late updates, odd intervals and values spanning both halves of 64-bit timing. The fixed-point fractions at the original endpoint and maximum prediction endpoint are also checked.
3. All four real x86 instruction forms execute before and after operand redirection in an allocated test image. Menu reapplication accepts the exact installed form or a restored original and preserves page protection.
4. A mismatch at the last instruction prevents changes at all four sites, both on initial application and reapplication.
5. Injected memory-protection failures before writes and during restoration leave the previous bytes and original page protection restored.
6. An incorrect multimedia-clock import or simulation interval prevents all clock writes; a matching import and 40 ms interval pass.
7. The SHA-256 implementation matches a standard known vector and, in this local run, the supplied original D2Client file.

The new tests are in `d2fps/src/mxl_timing_tests.rs`. They do not open another process or execute the game.

## Binary and loader checks

The four opcode/operand signatures, existing old/new import slots and ASLR relocation entries were compared with the actual original D2Client PE file from the tested installation. The client timestamp and image size also match the guard. Proprietary game binaries are not distributed.

A separate native 32-bit test executable loaded the final DLL successfully, allowing its normal `DllMain` to run, and resolved all three loader exports. It did **not** call game initialization, load the game or run a multiplayer session.

The existing `d2interface/src/module.rs` emits pointer-provenance warnings on this Rust version. Those upstream source files were not changed for the timing fork; the build and tests completed successfully. No new timing-module compiler warnings remained.

## Prior-build Median XL startup check

Before the repository cleanup, the user installed and ran a build with the same timing calculations. Its startup log from 2026-09-05 confirmed:

- The installed file matched that build's verified SHA-256 (`fd919e89c7ed057ca80a0edccc6554f0e31a9ab468d2e657392335fb36f7700d`).
- Game version 1.13c was detected and normal menu/game/motion-smoothing features applied.
- `MXL timing ACTIVE`: all four client clocks verified, with the realm prediction extension enabled.
- Client clocks were rechecked/reapplied successfully after menu initialization.
- D2GL loaded the fork through its existing D2FPS entry.
- The existing D2FPS config remained byte-identical, including the 450 FPS target.

See the [startup log excerpt](STARTUP-VERIFICATION.txt). The `arcane background` patch warning was also present in the original D2FPS startup log and is not new to the fork.

This verifies the timing behavior's startup in that installation. A measured multiplayer smoothness improvement, long-session stability and compatibility with other game builds have not been established.
