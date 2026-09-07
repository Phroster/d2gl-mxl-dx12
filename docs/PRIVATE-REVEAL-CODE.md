# Located act-reveal path (supported MXL 2.14.0 binary)

This is an offline inspection of the user's installed DLLs, not source supplied by the Median XL developers. Addresses below are RVAs, relative to the loaded module base. The private probe checks the supported file hashes and relocated code signatures.

The active `hotkey_revealmap` registration at D2Sigma+0x3DE0F6 references the string at +0x1F7BE8 and the callback at +0x68AB0. That callback jumps directly to +0x88340.

| Module / RVA | Observed role |
| --- | --- |
| D2Sigma +0x88340 | Validates the player's act, checks the per-act revealed flag, sets it, iterates matching level records, generates missing level layouts, selects map layers and restores the current layer afterward. |
| D2Sigma +0x884E0 | Walks the level's room list and calls the room function once per room. |
| D2Sigma +0x88500 | Preserves MXL-specific excluded rooms, loads missing room data, reveals walls and preset-object icons, then removes only the room data it temporarily loaded. |
| D2Common +0x2E360 / ordinal 10322 | Level generation called when the room list is absent. |
| D2Common +0x3CCA0 / ordinal 10401 | Temporary room-data loading. |
| D2Common +0x3CBE0 / ordinal 11099 | Corresponding temporary room-data removal. |

The complete room function is a candidate atomic chunk because it performs its cleanup before returning. The outer loop also generates whole levels, so splitting only room traversal might leave significant synchronous work. Beta 4 measures both before a behavior-changing patch is chosen.

A gradual reveal would need to preserve MXL's room exclusions, map-layer selection/restoration, per-act completion flags and temporary-room cleanup. It would also need to handle leaving the game or changing acts without retaining invalid room pointers. These requirements are not solved by running the original function on a background thread.

The native probe test verifies x86 calling conventions, parameters, return registers, last-error preservation, stack integrity, nested phase output and cleanup when the original callback throws. It uses synthetic functions and does not execute game DLLs outside the game.

Supported SHA-256:

- D2Sigma.dll: `FF44257078D994809D6B1A5A3A28657A75B1BB75395B1EE0714361D78355B728`
- D2Common.dll: `59FA5928522F566F2BF99675571206AD70DF889C89D3D07FA87EDF5083E06E10`

## Beta 4 result and beta 5 choice

The Act III capture `20260907-042115-pid25736` measured 1,237.50 ms inside full-act reveal. Level generation accounted for 708.57 ms; Caldeum (level 137) alone took 643.98 ms. Per-level room traversal took another 346.92 ms. These two scopes are separate; the nested individual room/load/unload timings must not be added again. Seventeen diagnostic records were dropped. About 182 ms of the root duration was outside these measured generation/traversal scopes and is not yet specifically attributed.

Caldeum's 105 observed rooms were already resident after generation. That does not mean the level can be omitted: its preset is not one of the room worker's exclusions. Splitting room work alone would leave the single 644 ms generation call intact.

The user chose automatic full-act reveal on act entry. Beta 5 posts one registered window message after a completed gameplay frame. Before calling the original root it revalidates the player's act/room chain, tables, map layer and original completion flag. No game pointers are dereferenced by a background thread, and no room pointer is saved in a pending message. Act/session pointer identities are only compared; fresh pointers are read before execution. Original game state owns the completion tracking, so new games that reuse addresses are handled correctly. A failed/unconfirmed callback is not retried every frame. The first safe message dispatch may be after the loading screen, so this is a timing change, not eliminated computation.

On the supported layout, readiness requires the path's Room1 -> Room2 -> Level -> ActMisc -> Act chain to match the player's current act. The snapshot reads are guarded against invalid pointers. The original routine itself is not wrapped in an exception-swallowing handler: its exceptions still propagate, with our busy guard restored during unwind. Manual T is forwarded unchanged.

Readiness diagnostics (`auto_reveal_wait_reason`) are emitted only when the reason changes: 0 ready, 1 not a gameplay frame, 2 global pointers, 3 player/tables/map layer, 4 act/path, 5 act relationship, 6 current Room1, 7 Room2 relationship, 8 current level, 9 level tables, 10 completion flag. A guarded read fault adds 100 to the stage number. These values make an overly strict readiness check visible without repeated log traffic.

Beta 5 native verification passed: automatic-reveal scheduling scenarios, the original six-hook ABI/exception test, and the silent DirectSound/GPU/concurrent-logger test. This verifies the built code with synthetic state and native Windows messages; it does not substitute for the first live act-entry run.
