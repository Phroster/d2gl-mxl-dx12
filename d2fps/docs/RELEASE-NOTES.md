# Historical D2FPS development build

This source workspace contains the Rust timing integration that preceded the combined **[MXL Smooth Motion package](../../README.md)**. It is retained for development and comparison.

The old standalone release has been retired. Use the [current 1.0 release](https://github.com/Phroster/mxl-smooth-motion/releases/tag/v1.0) for player downloads. The combined package runs the official D2FPS engine and applies the correction through D2GL.

Historical standalone build identity:

- Version: 1.0; DLL size: 387072 bytes.
- SHA-256: `3b54d5b7c30c4001cfde7e5f8a183db041fb1806182709ddc92f790a7085791f`.
- Seven tests covering 378 timing cases.
- Source-level precise clocks and bounded realm prediction.

These identify the retained development implementation, not the DLL shipped by the current player package.
