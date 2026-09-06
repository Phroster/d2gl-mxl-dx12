# D2FPS source workspace

This is the D2FPS source component of **[MXL Smooth Motion](../README.md)**, based on [Jarcho's D2FPS](https://github.com/Jarcho/d2-rs).

**Players: use the [MXL Smooth Motion download and installation guide](../README.md#install-in-six-steps).** The current package keeps the official D2FPS engine and applies the timing correction through D2GL. Do not replace that official DLL with this optional development build.

This workspace retains the Rust implementation of precise client timing and bounded realm prediction for development, tests and comparison with the integrated renderer implementation.

- [Timing implementation](docs/MXL-TIMING.md)
- [Build instructions and historical standalone validation](docs/DEVELOPER-GUIDE.md)
- [Original D2FPS documentation](d2fps/README.md)
- [Current package architecture](../docs/ARCHITECTURE.md)

Original authorship and licenses are retained. D2FPS and its timing integration use [GPL-3.0](LICENSE-GPL.txt); the supporting patch crates retain their MIT/Apache notices.
