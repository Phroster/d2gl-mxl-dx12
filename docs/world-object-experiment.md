# Object indicators (local experiment)

Set `ObjectLabels=1` under `[NativeLoot]` in `mxl-native-loot.ini` and restart.
Set it to `0` to return to the normal item-only display.

Ordinary chests, urns, barrels, racks and other usable objects get a small
native glint. Locked, large and special containers also get a readable name;
special containers have a stronger glow. Usable shrines get a name and pulse.
Wells have a quiet cue; waypoints and your stash have a name. Exploding
containers use a red cue. Opened/broken loot containers, spent shrines,
doors and non-interactive scenery are excluded. Existing dropped-item labels are unchanged.

Names share the loot layout. Object names and effects are clickable when
`ClickEffects=1`. Actual dropped items and native character targets
keep priority. The game still handles walking, opening and activation normally.

The experiment keeps current-frame snapshots for up to 1024 visible objects;
the twelve-name limit applies only to labels, not ordinary object cues.
It does not scan rooms or retain unit pointers. It reuses the existing immutable
native pulse cells and font atlas. No new artwork, file loading during play,
automatic gameplay actions, or diagnostic recording is added. Object metadata is read
only after the existing checks for the supported game DLLs have passed.
