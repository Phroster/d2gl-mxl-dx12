# MXL Smooth Motion DX12 architecture

The main renderer converts the game's Glide or DirectDraw calls into native Direct3D 12 work. Both wrapper DLLs contain the same multiplayer timing correction.

## Rendering

- DX12 textures, buffers, pipeline states, command lists and resource barriers.
- Reusable upload memory, fenced resource lifetimes and cached binding state.
- DXGI flip presentation, a short frame queue, and V-Sync/tearing support.
- D2GL's original graphics options and assets, including the unchanged MPQ.
- Native DX12 rendering for Dear ImGui and the Ctrl+O menu.
- An FPS tab that saves D2FPS settings for the next game launch.

Shader sources are translated through glslang and SPIRV-Cross, then compiled to DirectX bytecode. The source's internal OpenGL-shaped calls are implemented by the DX12 backend; no OpenGL driver context is created.

## Timing and D2FPS

The official `d2fps.dll` stays on disk and supplies the FPS engine. The renderer loads it and applies the guarded correction in memory. This preserves the Median XL launcher's normal D2FPS file check.

The correction replaces five clock operands and extends the upper interpolation bound only for realm multiplayer. The original 40 ms simulation interval, negative-time path, SP/LAN bound and authoritative position restoration are retained.

Exact supported SHA-256 hashes:

| File | SHA-256 |
|---|---|
| D2Client.dll | `dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906` |
| Official d2fps.dll | `db9de4d4d320a7b70e66fe6b4aaa0e6f1560a5300a4993cc81cf4512ab1240c1` |

D2Sigma must be loaded. File identity, patch instructions, clock imports and the simulation interval are checked before applying the fix. The status reports On after successful application and readback.

## Rendering performance and map reveal

The renderer uploads each buffer's current live range once per frame/version instead of repeatedly copying growing prefixes for later draws. Partial updates retain the untouched tail. This removes the excessive upload/spill work measured during crowded fights.

Automatic act reveal calls Median XL's original routine on the game thread before the first scene draw when the guarded native entry point is available, including the supported official D2FPS patch. A queued window message after a completed gameplay frame remains the compatibility fallback. It validates the current player/act/room state and honors Median XL's own completion flags. It moves the complete act reveal to entry; it does not accelerate level generation. D2Sigma, D2Common and the selected client/FPS path must pass their file and code guards.

The upload fix, guarded automatic reveal, asset caches and sound-cancellation wakeup activate automatically. A process-wide owner prevents duplicate reveal initialization across the two renderer DLLs.

Repeated tile loads reuse native archive handles and requested 4 KiB sectors within one producer frame. Only the verified D2CMP DT1 block-loader call site is eligible. The cache starts after frame construction begins and releases idle handles and memory before frame submission. It holds at most eight files, sixteen native handles and 16 MiB of file data, with a 4 MiB per-file limit. The first block read stays native; later block opens can share previously decoded sectors. It never reads unrelated sectors or eagerly loads the entire file. An independent native reader keeps fill failures away from the caller's cursor. EOF-reaching reads, repeated reads on the same open, unknown flags, loose files and capacity misses use the native reader. Outstanding caller-owned handles survive a frame boundary. Set `tile_file_cache=0` under `[Other]` in the game's `d2gl.ini` to disable it at the next launch.

The tile cache additionally checks the installed Fog, Storm, D2CMP and D2Sound file identities, native ABI/caller code spans and imports. A failed guard disables caching. The six shared archive import slots are atomically replaced only after their original pointers are verified. Source/caller checks keep sound, animation-header and other requests outside the tile data cache.

During each synchronous D2CMP file open, the archive hash cache reuses Storm's three native filename hash results while Storm checks successive archives. The original archive order, locale/platform selection, open result, handles and file contents remain native. Its scope ends when that single open returns or unwinds, including native SEH; it retains no filename or archive selection across opens. Only the three verified lookup call sites, modes 0–2, and the same unchanged printable ASCII name pointer qualify. Nested opens have independent scopes; unknown callers, encryption-key hashes, other names and all D2Sound requests follow the original hash function. The cache verifies the Storm file identity, the complete relocated hash routine and lookup call bytes before a thread-enrolled Detours transaction. It preserves the native volatile-register and arithmetic-flag outputs as well as the stack. `archive_hash_cache=0` under `[Other]` disables it at the next launch; it is independent of `tile_file_cache`.

Unused queued sounds are released by the client through Fog's native async free routine. On the supported DLLs, Storm cancellation removes the corresponding pending requests and queues completion notifications without waking its worker. Fog then waits for the completion event before releasing the job. The guarded sound-cancellation fix wakes that existing worker after native cancellation, so completion can be dispatched promptly. It keeps the original cancellation, completion wait, memory release and sound output.

The wakeup is restricted to the verified client sound-release call and Fog's cancellation call on the same thread. A scoped context is restored across nested calls and native exceptions. After cancellation, the verified Storm priority helper has no matching pending request to alter; its final event signal wakes the worker. File identities, two original import pointers and six relocated code spans guard the path. Unsupported code follows the original implementation. No job ownership moves to another thread and no allocations are freed early.

This fix is enabled for `Game.exe`. Set `[Other] sound_cancel_wakeup=0` in `d2gl.ini` to disable it on the next launch.

## Launcher and ReShade

Enable both custom graphics DLL checkboxes under the launcher's **Unofficial Graphic Drivers** section. The unchanged MPQ matches the inspected official distribution.

ReShade uses its DirectX installation. ReShade and personal presets are not bundled with the mod.

The timing integration uses the official D2FPS engine already supplied by Median XL. Historical standalone source remains available in the Git history.

[Build and tests](BUILD.md) · [Upstream versions](UPSTREAM.json)
