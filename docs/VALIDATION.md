# MXL Smooth Motion DX12 validation

DX12 is the main implementation. The owner reports the version working well, including ReShade and the Ctrl+O menu. The 1.0 package retains the exact accepted renderer DLLs.

The native GPU image checks, menu test, 168 timing cases, 14 built-in shader stages and 238 MPQ shader stages passed. Main-install startup verified native DX12, ReShade on DirectX and all six smoothing changes.

[Detailed checks and scope](../experimental/VALIDATION.md) · [Binary identities](BINARY-VERIFICATION.json) · [Checkpoint record](../experimental/checkpoints/2026-09-06.json)

No numerical performance gain or universal hardware result is claimed.
