# Settings and the MPQ

## Frame pacing

`d2fps.ini` keeps the original D2FPS format. `fps=0` selects the monitor's refresh rate; a number such as `fps=141` selects a manual target. For a 144 Hz VRR display, 141 is a useful target to try for refresh-rate headroom. It is not a fix for unrelated multi-monitor G-Sync problems. `bg-fps=25` reduces background rendering work.

The installer applies automatic refresh matching unless `-PreserveFrameRate` is supplied. Both choices remain editable after installation.

D2GL's foreground/background limiters and its separate motion prediction are disabled when the managed D2FPS engine initializes. This avoids overlapping limiters or two movement prediction systems. `frame_latency=1` keeps the shortest configured render queue. No renderer scheduling experiment or NVIDIA driver override from earlier investigations is included.

## Visuals

The default D2GL template retains the tested xBRZ shader, HD text/cursor, sharpening, FXAA and bloom settings. Existing visual values are preserved by the installer; the default template is used when an INI is missing. Those settings can be adjusted in `d2gl.ini` or the existing Ctrl+O menu.

ReShade and its presets are not part of this package and are not modified. Image-quality settings have not been reduced to manufacture an FPS improvement.

## Smoothing

The additional multiplayer correction is automatic, with no menu toggle or INI switch. It uses a more precise clock and permits at most 20 ms of extra visual prediction in realm multiplayer. The simulation remains 25 Hz. SP/LAN keep the original interpolation bound; the shared client clock correction also affects SP.

The existing D2FPS `motion-smoothing` option remains available. If a user disables D2FPS's movement smoothing, its entity interpolation is not drawn. The integrated correction itself still initializes against the supported files.

## d2gl.mpq

This is an asset archive, not another FPS mod. D2GL reads files inside it through Diablo II's archive functions. It includes assets used by the shader selection, HD fonts and other visual features. It belongs beside `glide3x.dll`/`ddraw.dll` in the actual game folder.

The bundled file is byte-identical to the asset archive from the imported MXL D2GL source and the tested installation:

`f6c85e6f0b77df3524dd7f4aafa66fd81ddf505907e363b531a6d47b0a7d201a`

There is no need to edit or rebuild it for the timing fix. Most users should only edit the INIs or use the graphics menu.

The launcher also verifies this MPQ. Keeping the matching archive unchanged preserves that check; the current package matches the official MXL 2.14.0 manifest SHA-1 as well as the source archive hash.

## Compatibility defaults

Keep `integrity-checks=true` and `reapply-patches=true`. The additional correction has its own exact binary/instruction guards. `arcane-bg=false` avoids requesting an optional D2FPS patch that is already incompatible with the tested MXL client.
