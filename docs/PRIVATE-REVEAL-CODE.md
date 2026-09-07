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
