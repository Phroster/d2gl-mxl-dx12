# Private diagnostic beta 3

This keeps the current DX12 renderer, ReShade support and multiplayer smoothing fix, and adds measurements for the remaining crowded-combat drops. It is a private test build, separate from public 1.0.

Beta 2 fixes the repeated geometry uploads found in the first capture. It uploads the current data range once and reuses it for later draws, while preserving partial-buffer updates. The first trace's 75-85 FPS frames spent a median 10.8 ms in uploads, copying about 98 MB per frame and spilling beyond the 64 MB pool; their own GPU work took about 2.35 ms.

The map-reveal delay is separate. T key-down/up and time spent handling those messages are now timestamped to identify the next reveal precisely. The key is forwarded unchanged. CSV reports are also readable while recording, so live analysis no longer requires stopping the capture.

Beta 3 adds `input.csv` for T key-down: wall time, the game thread's CPU time/cycles, process I/O and page-fault changes, and the forwarded window-procedure module/offset. These extra queries run only on T, not every frame. CPU time versus wall time separates active work from time spent waiting or descheduled. I/O counters cover the whole process and include cached reads; page faults include soft faults, so neither is proof of slow storage. Missing counters are marked unavailable. Up to 4,096 T profiles are retained per session. The renderer and upload fix are unchanged.

Replace only `glide3x.dll` and `ddraw.dll`, and add `mxl-diagnostics.ini` beside Game.exe. Keep your existing graphics/FPS settings, MPQ, official D2FPS, ReShade and hotkeys. A full game restart is required.

Logging starts automatically. Play normally, including a crowded fight. Tell us when the drop happens and whether sound was on. Reports appear in `mxl-diagnostics/<session>/` in the game folder. No recording software or extra DLL is needed.

The logger records frame intervals, game-side handoff waits, render-thread wall time, our DX12 GPU time, GPU/Present waits, draw/texture/buffer activity, shader and pipeline creation, and DirectSound activity. A foreground gameplay frame over 10 ms is marked slow. Loading screens, menus and Alt-Tab are marked separately.

Audio calls are forwarded unchanged. A temporary silent DirectSound device and buffers discover the method addresses at startup; they are never played and never change the primary format or cooperative level. Hook discovery is then frozen: combat never suspends threads to add hooks. The file reports hook coverage/failures and any unfamiliar interface variant left unmeasured. Active sound calls over 0.5 ms, or failed calls, get individual timestamps. Per-second summaries also count the fast calls.

Frame/audio threads only update counters or try to enqueue a fixed-size record. They never wait for the disk logger. A lower-priority background writer keeps at most three 32 MiB CSVs per session. If it cannot keep up, it drops records and reports the number rather than blocking gameplay. GPU query results are read only after an existing frame fence is complete, without an additional GPU wait.

Create an empty file named `STOP` inside the current session folder to stop logging without exiting the game. To start a fresh session, restart the game. Set `enabled=0` in `mxl-diagnostics.ini` before launch for a logging-off comparison; `audio=0` disables the audio instrumentation on the next launch.

`session.txt` explains the columns and `status.txt` confirms logging is running. The analysis helper is `scripts/analyze-diagnostics.py`:

```powershell
python scripts/analyze-diagnostics.py 'G:\Median XL\median-xl\mxl-diagnostics\SESSION'
```

The GPU timings cover our command lists; ReShade may submit additional GPU work inside its Present hook. Render timings are nested, not additive. Audio durations measure time inside the sound API calls; they do not cover every operation inside D2Sound or prove an audio-driver fault. Missing hook coverage is reported explicitly. These measurements guide the next fix; this beta makes no speculative graphics/audio tuning changes.
