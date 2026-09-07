# MXL Smooth Motion DX12 validation

DX12 is the main implementation. Version 1.1 includes the tested crowded-scene upload fix and automatic act reveal. The owner confirmed the beta gameplay and Act III reveal behavior before release.

The native GPU image checks, menu test, 168 timing cases, 14 built-in shader stages and 238 MPQ shader stages passed. Main-install startup verified native DX12, ReShade on DirectX and all six smoothing changes.

[Detailed checks and scope](../experimental/VALIDATION.md) · [Binary identities](BINARY-VERIFICATION.json) · [Checkpoint record](../experimental/checkpoints/2026-09-06.json)

No numerical performance gain or universal hardware result is claimed.

## Changes tested for 1.1

The first crowded-area capture identified repeated uploads as the cause of its 75-85 FPS frames: those frames copied about 98 MB each, with median upload time 10.8 ms. The fix reuses each buffer's live upload. Native readback tests verify identical GPU bytes and correct preserved tails after partial updates. Later gameplay captures showed the improvement, but are not controlled hardware benchmarks.

The live Act III test completed automatic reveal in 1,221.35 ms on entry. The three subsequent T handlers took 0.0563, 0.0533 and 0.0522 ms. This moves the cost to entry; it does not remove level-generation work. Other act/session transitions are covered by synthetic native tests.

Release checks cover reveal calling conventions, readiness, new/revisited acts, reused session addresses, manual-T races, stale messages, nested drawing and exception cleanup. Logging-off tests use an isolated Game.exe with both a missing INI and the supplied disabled INI; automatic reveal still runs, no diagnostic directory is created, and no audio timing hooks are installed. Recording-on tests retain DirectSound forwarding, GPU timestamps and the nonblocking writer. Renderer upload, Glide, menu and timing tests also run for the release.
